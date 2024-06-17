// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QMetaEnum>
#include <QProcessEnvironment>

#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QGuiApplication>

#include <cstdlib>
#include <cstdio>
#include <functional>
#include <memory>

#include "global.h"
#include "logging.h"
#include "qtyaml.h"
#include "configcache.h"
#include "utilities.h"
#include "exception.h"
#include "utilities.h"
#include "configuration.h"
#include "configuration_p.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// helper functions to (de)serialize std::chrono::durations via QDataStream
template <typename T, typename U>
QDataStream &operator<<(QDataStream &ds, const std::chrono::duration<T, U> &duration)
{
    ds << qint64(duration.count());
    return ds;
}

template <typename T, typename U>
QDataStream &operator>>(QDataStream &ds, std::chrono::duration<T, U> &duration)
{
    qint64 count;
    ds >> count;
    duration = std::chrono::duration<T, U> { count };
    return ds;
}


// helper class that makes it possible to handle QDataStream << and >> in a single function
class SerializeStream {
public:
    SerializeStream(QDataStream &ds, bool write)
        : m_ds(ds)
        , m_write(write)
    { }

    template <typename T> SerializeStream &operator&(T &&t)
    {
        if (m_write)
            m_ds << std::forward<T>(t);
        else
            m_ds >> std::forward<T>(t);

        return *this;
    }

    QDataStream &m_ds;
    bool m_write;
};

// helper class to serialize lists within the configuration using the SerializeStream mechanism
template <typename LIST, typename ...MEMBER_PTRS>
class SerializeList
{
public:
    SerializeList(LIST &list, MEMBER_PTRS... memberPtrs)
        : m_list(list)
        , m_memberPtrs(memberPtrs...)
    { }

    QDataStream &serialize(QDataStream &ds)
    {
        // C++ doesn't allow parameter pack expansion directly for the member function calls.
        // So the ptr-to-members to stream-member-values operation has to been in two steps:
        //   1) convert the ptr-to-member pack into a member-value pack
        //   2) parameter expand THAT pack into operator << to stream the values
        ds << m_list.size();
        for (const auto &entry : m_list) {
            std::apply([&](auto... memberPtrs) {
                [](QDataStream &ds, auto &&... resolvedMembers) {
                    (ds << ... << resolvedMembers);
                }(ds, entry.*memberPtrs...);
            }, m_memberPtrs);
        }
        return ds;
    }

    QDataStream &deserialize(QDataStream &ds)
    {
        m_list.clear();
        qsizetype s;
        ds >> s;
        m_list.reserve(s);
        for (qsizetype i = 0; i < s; ++i) {
            typename LIST::value_type entry;
            std::apply([&](auto... memberPtrs) {
                [](QDataStream &ds, auto &&... resolvedMembers) {
                    (ds >> ... >> resolvedMembers);
                }(ds, entry.*memberPtrs...);
            }, m_memberPtrs);
            m_list.append(entry);
        }
        return ds;
    }

private:
    LIST &m_list;
    std::tuple<MEMBER_PTRS...> m_memberPtrs;
};

template <typename ENTRY, typename ...MEMBER_PTRS>
QDataStream &operator<<(QDataStream &ds, SerializeList<ENTRY, MEMBER_PTRS...> &&serialize)
{
    return serialize.serialize(ds);
}

template <typename ENTRY, typename ...MEMBER_PTRS>
QDataStream &operator>>(QDataStream &ds, SerializeList<ENTRY, MEMBER_PTRS...> &&serialize)
{
    return serialize.deserialize(ds);
}


// helper functions to merge configuration fields when loading multiple YAML files
template <typename T> void mergeField(T &into, const T &from, const T &def)
{
    if (from != def)
        into = from;
}

void mergeField(QVariantMap &into, const QVariantMap &from, const QVariantMap & /*def*/)
{
    recursiveMergeVariantMap(into, from);
}

template <typename T> void mergeField(QList<T> &into, const QList<T> &from, const QList<T> & /*def*/)
{
    into.append(from);
}

template <typename T, typename U> void mergeField(QMap<T, U> &into, const QMap<T, U> &from, const QMap<T, U> & /*def*/)
{
    into.insert(from);
}

// ordered map
void mergeField(QList<std::pair<QString, QString>> &into, const QList<std::pair<QString, QString>> &from,
                const QList<std::pair<QString, QString>> & /*def*/)
{
    for (auto &p : from) {
        auto it = std::find_if(into.begin(), into.end(), [p](const auto &fp) { return fp.first == p.first; });
        if (it != into.end())
            it->second = p.second;
        else
            into.append(p);
    }
}


// the templated adaptor class needed to instantiate ConfigCache<ConfigurationData> in parse() below
template<> class ConfigCacheAdaptor<ConfigurationData>
{
public:
    static ConfigurationData *loadFromSource(QIODevice *source, const QString &fileName)
    {
        auto cd = std::make_unique<ConfigurationData>();
        ConfigurationPrivate::loadFromSource(source, fileName, *cd);
        return cd.release();
    }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName)
    {
        sourceContent = ConfigurationPrivate::substituteVars(sourceContent, fileName);
    }
    ConfigurationData *loadFromCache(QDataStream &ds)
    {
        auto cd = std::make_unique<ConfigurationData>();
        ConfigurationPrivate::loadFromCache(ds, *cd);
        return cd.release();
    }
    void saveToCache(QDataStream &ds, const ConfigurationData *cd)
    {
        ConfigurationPrivate::saveToCache(ds, *cd);
    }
    static void merge(ConfigurationData *to, const ConfigurationData *from)
    {
        ConfigurationPrivate::merge(*from, *to);
    }
};


Configuration::Configuration(const char *additionalDescription,
                             bool onlyOnePositionalArgument)
    : Configuration(QStringList(), u":/build-config.yaml"_s,
                    additionalDescription, onlyOnePositionalArgument)
{ }

Configuration::Configuration(const QStringList &defaultConfigFilePaths,
                             const QString &buildConfigFilePath,
                             const char *additionalDescription,
                             bool onlyOnePositionalArgument)
    : d(new ConfigurationPrivate)
    , yaml(d->data)
{
    d->defaultConfigFilePaths= defaultConfigFilePaths;
    d->buildConfigFilePath = buildConfigFilePath;
    d->onlyOnePositionalArgument = onlyOnePositionalArgument;

    // using QStringLiteral for all strings here adds a few KB of ro-data, but will also improve
    // startup times slightly: less allocations and copies. MSVC cannot cope with multi-line though

    const char *description =
        "In addition to the command line options below, the following environment\n"
        "variables can be set:\n\n"
        "  AM_STARTUP_TIMER       If set to '1', a startup performance analysis is\n"
        "                         printed to the console. Anything other than '1' is\n"
        "                         interpreted as the name of a file to use, instead\n"
        "                         of the console.\n"
        "\n"
        "  AM_FORCE_COLOR_OUTPUT  Can be set to 'on' to force color output to the\n"
        "                         console and to 'off' to disable it. Any other value\n"
        "                         enables the default, auto-detection behavior.\n"
        "\n"
        "  AM_NO_CUSTOM_LOGGING   If set to '1', debug output is not redirected to DLT,\n"
        "                         colorized or nicely formatted.\n"
        "\n"
        "  AM_NO_DLT_LOGGING      If set to '1', debug output is not redirected to DLT.\n"
        "\n"
        "  AM_NO_CRASH_HANDLER    If set to '1', no crash handler is installed. Use\n"
        "                         this, if the application manager's crash handler is\n"
        "                         interfering with other debugging tools you are using.\n";

    d->clp.setApplicationDescription(u"\n"_s + QCoreApplication::applicationName() + u"\n\n"_s
                                    + (additionalDescription ? (QString::fromLatin1(additionalDescription) + u"\n\n"_s) : QString())
                                    + QString::fromLatin1(description));

    d->clp.addOption({ { u"h"_s, u"help"_s
#if defined(Q_OS_WINDOWS)
                         , u"?"_s
#endif
                     },                         u"Displays this help."_s });
    d->clp.addOption({ u"version"_s,              u"Displays version information."_s });
    QCommandLineOption cf { { u"c"_s, u"config-file"_s },
                          u"Load configuration from file (can be given multiple times)."_s, u"files"_s };
    cf.setDefaultValues(d->defaultConfigFilePaths);
    d->clp.addOption(cf);
    d->clp.addOption({ { u"o"_s, u"option"_s },   u"Override a specific config option."_s, u"yaml-snippet"_s });
    d->clp.addOption({ { u"no-cache"_s, u"no-config-cache"_s },
                     u"Disable the use of the config and appdb file cache."_s });
    d->clp.addOption({ { u"clear-cache"_s, u"clear-config-cache"_s },
                     u"Ignore an existing config and appdb file cache."_s });
    if (!buildConfigFilePath.isEmpty())
        d->clp.addOption({ u"build-config"_s,     u"Dumps the build configuration and exits."_s });

    d->clp.addPositionalArgument(u"qml-file"_s,   u"The main QML file."_s);
    d->clp.addOption({ u"log-instant"_s,          u"Log instantly at start-up, neglect logging configuration."_s });
    d->clp.addOption({ u"builtin-apps-manifest-dir"_s, u"Base directory for built-in application manifests."_s, u"dir"_s });
    d->clp.addOption({ u"installation-dir"_s,     u"Base directory for package installations."_s, u"dir"_s });
    d->clp.addOption({ u"document-dir"_s,         u"Base directory for per-package document directories."_s, u"dir"_s });
    d->clp.addOption({ u"dbus"_s,                 u"Register on the specified D-Bus."_s, u"<bus>|system|session|none|auto"_s, u"auto"_s });
    d->clp.addOption({ u"fullscreen"_s,           u"Display in full-screen."_s });
    d->clp.addOption({ u"no-fullscreen"_s,        u"Do not display in full-screen."_s });
    d->clp.addOption({ u"I"_s,                    u"Additional QML import path."_s, u"dir"_s });
    d->clp.addOption({ { u"v"_s, u"verbose"_s },  u"Verbose output."_s });
    d->clp.addOption({ u"slow-animations"_s,      u"Run all animations in slow motion."_s });
    d->clp.addOption({ u"no-security"_s,          u"Disables all security related checks (dev only!)"_s });
    d->clp.addOption({ u"development-mode"_s,     u"Enable development mode, allowing installation of dev-signed packages."_s });
    d->clp.addOption({ u"disable-watchdog"_s,     u"Disables all watchdogs, useful for debugging."_s });
    d->clp.addOption({ u"no-dlt-logging"_s,       u"Disables logging using automotive DLT."_s });
    d->clp.addOption({ u"force-single-process"_s, u"Forces single-process mode even on a wayland enabled build."_s });
    d->clp.addOption({ u"force-multi-process"_s,  u"Forces multi-process mode. Will exit immediately if this is not possible."_s });
    d->clp.addOption({ u"wayland-socket-name"_s,  u"Use this file name to create the wayland socket."_s, u"socket"_s });
    d->clp.addOption({ u"single-app"_s,           u"Runs a single application only (ignores the database)"_s, u"info.yaml file"_s }); // rename single-package
    d->clp.addOption({ u"logging-rule"_s,         u"Adds a standard Qt logging rule."_s, u"rule"_s });
    d->clp.addOption({ u"qml-debug"_s,            u"Enables QML debugging and profiling."_s });
    d->clp.addOption({ u"instance-id"_s,          u"Use this id to distinguish between multiple instances."_s, u"id"_s });

    d->clp.addOption({ { u"r"_s, u"recreate-database"_s }, u"Deprecated (mapped to --clear-cache."_s });
    d->clp.addOption({ u"installed-apps-manifest-dir"_s, u"Deprecated (ignored)."_s, u"dir"_s });
    d->clp.addOption({ u"app-image-mount-dir"_s,  u"Deprecated (ignored)."_s, u"dir"_s });
    d->clp.addOption({ u"disable-installer"_s,    u"Deprecated (ignored)."_s });
    d->clp.addOption({ u"disable-intents"_s,      u"Deprecated (ignored)."_s });
    d->clp.addOption({ u"database"_s,             u"Deprecated (ignored)."_s, u"file"_s });
    d->clp.addOption({ u"enable-touch-emulation"_s, u"Deprecated (ignored)."_s });
    d->clp.addOption({ u"load-dummydata"_s,       u"Deprecated (loads QML dummy-data)."_s });
    d->clp.addOption({ u"no-ui-watchdog"_s,       u"Deprecated (mapped to --watchdog=off)."_s });

    { // qmltestrunner specific, necessary for CI blacklisting
        QCommandLineOption qtrsf { u"qmltestrunner-source-file"_s, u"appman-qmltestrunner only: set the source file path of the test."_s, u"file"_s };
        qtrsf.setFlags(QCommandLineOption::HiddenFromHelp);
        d->clp.addOption(qtrsf);
    }
}

QVariant Configuration::buildConfig() const
{
    QFile f(d->buildConfigFilePath);
    if (f.open(QFile::ReadOnly)) {
        try {
            return YamlParser::parseAllDocuments(f.readAll()).toList();
        } catch (...) {
        }
    }
    return QVariant();
}

Configuration::~Configuration()
{ }

void Configuration::parseWithArguments(const QStringList &arguments)
{
    if (!d->clp.parse(arguments))
        throw Exception(d->clp.errorText());

    if (d->clp.isSet(u"version"_s))
        d->clp.showVersion();

    if (d->clp.isSet(u"help"_s))
        d->clp.showHelp();

    if (!d->buildConfigFilePath.isEmpty() && d->clp.isSet(u"build-config"_s)) {
        QFile f(d->buildConfigFilePath);
        if (f.open(QFile::ReadOnly)) {
            ::fprintf(stdout, "%s\n", f.readAll().constData());
            ::exit(0);
        } else {
            throw Exception("Could not find the embedded build config.");
        }
    }

#if defined(AM_TIME_CONFIG_PARSING)
    QElapsedTimer timer;
    timer.start();
#endif

    const QStringList rawConfigFilePaths = d->clp.values(u"config-file"_s);
    QStringList configFilePaths;
    configFilePaths.reserve(rawConfigFilePaths.size());
    for (const auto &path : rawConfigFilePaths) {
        if (QFileInfo(path).isDir()) {
            const auto entries = QDir(path).entryInfoList({ u"*.yaml"_s }, QDir::Files, QDir::Name);
            for (const auto &entry : entries)
                configFilePaths << entry.filePath();
        } else {
            configFilePaths << path;
        }
    }

    AbstractConfigCache::Options cacheOptions = AbstractConfigCache::MergedResult;
    if (noCache())
        cacheOptions |= AbstractConfigCache::NoCache;
    if (clearCache())
        cacheOptions |= AbstractConfigCache::ClearCache;

    if (configFilePaths.isEmpty()) {
        d->data = { };
    } else {
        ConfigCache<ConfigurationData> cache(configFilePaths, u"config"_s, { 'C','F','G','D' },
                                             ConfigurationPrivate::dataStreamVersion(), cacheOptions);

        cache.parse();
        if (auto result = cache.takeMergedResult()) {
            d->data = *result;
            delete result;
        } else {
            d->data = { };
        }
    }

    const QStringList options = d->clp.values(u"o"_s);
    for (const QString &option : options) {
        QByteArray yaml("formatVersion: 1\nformatType: am-configuration\n---\n");
        yaml.append(option.toUtf8());
        QBuffer buffer(&yaml);
        buffer.open(QIODevice::ReadOnly);
        try {
            ConfigurationData *cd = ConfigCacheAdaptor<ConfigurationData>::loadFromSource(&buffer, u"command line"_s);
            if (cd) {
                ConfigCacheAdaptor<ConfigurationData>::merge(&d->data, cd);
                delete cd;
            }
        } catch (const Exception &e) {
            throw Exception("Could not parse --option value: %1").arg(e.errorString());
        }
    }

    // early sanity checks
    if (d->onlyOnePositionalArgument && (d->clp.positionalArguments().size() > 1))
        throw Exception("Only one main qml file can be specified.");

    // merge in the command-line options that map to YAML fields
    {
        ConfigurationData clcd;

        auto configIfSet = [&](const QString &clp, auto &cd) {
            if (d->clp.isSet(clp)) {
                if constexpr (std::is_same_v<decltype(cd), QString &>)
                    cd = d->clp.value(clp);
                else if constexpr (std::is_same_v<decltype(cd), bool &>)
                    cd = true;
                else if constexpr (std::is_same_v<decltype(cd), QStringList &>)
                    cd = d->clp.values(clp) + cd;
                else
                    static_assert(QtPrivate::value_dependent_false<cd>(), "Unsupported type"); // CWG2518
            }
        };

        configIfSet(u"instance-id"_s,          clcd.instanceId);
        configIfSet(u"fullscreen"_s,           clcd.ui.fullscreen);
        configIfSet(u"I"_s,                    clcd.ui.importPaths);
        configIfSet(u"builtin-apps-manifest-dir"_s, clcd.applications.builtinAppsManifestDir);
        configIfSet(u"installation-dir"_s,     clcd.applications.installationDir);
        configIfSet(u"document-dir"_s,         clcd.applications.documentDir);
        configIfSet(u"load-dummydata"_s,       clcd.ui.loadDummyData);
        configIfSet(u"no-security"_s,          clcd.flags.noSecurity);
        configIfSet(u"development-mode"_s,     clcd.flags.developmentMode);
        configIfSet(u"no-ui-watchdog"_s,       clcd.watchdog.disable); // legacy
        configIfSet(u"disable-watchdog"_s,     clcd.watchdog.disable);
        configIfSet(u"force-single-process"_s, clcd.flags.forceSingleProcess);
        configIfSet(u"force-multi-process"_s,  clcd.flags.forceMultiProcess);
        configIfSet(u"logging-rule"_s,         clcd.logging.rules);

        if (d->clp.isSet(u"no-fullscreen"_s))
            clcd.ui.fullscreen = false;

        ConfigurationPrivate::merge(clcd, d->data);

        const auto args = d->clp.positionalArguments();
        if (!args.isEmpty())
            d->data.ui.mainQml = args.at(0);

        QStringList importPaths = d->data.ui.importPaths;
        for (int i = 0; i < d->data.ui.importPaths.size(); ++i)
            d->data.ui.importPaths[i] = toAbsoluteFilePath(d->data.ui.importPaths.at(i));
    }

    if (!d->data.instanceId.isEmpty()) {
        try {
            validateIdForFilesystemUsage(d->data.instanceId);
        } catch (const Exception &e) {
            throw Exception("Invalid instance-id (%1): %2\n").arg(d->data.instanceId, e.errorString());
        }
    }

#if QT_CONFIG(am_installer)
    if (d->data.applications.installationDir.isEmpty()) {
        qCWarning(LogDeployment) << "No --installation-dir command line parameter or applications/installationDir "
                                    "configuration key specified. It won't be possible to install, remove or "
                                    "access installable packages.";
    }
#endif
}

void ConfigurationPrivate::loadFromCache(QDataStream &ds, ConfigurationData &cd)

{
    serialize(ds, cd, false);
}

void ConfigurationPrivate::saveToCache(QDataStream &ds, const ConfigurationData &cd)
{
    Q_ASSERT(ds.device() && ds.device()->isWritable());
    serialize(ds, const_cast<ConfigurationData &>(cd), true);
}

quint32 ConfigurationPrivate::dataStreamVersion()
{
    return 16;
}

void ConfigurationPrivate::serialize(QDataStream &ds, ConfigurationData &cd, bool write)
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust both
    //            serialize() and merge() at the same time!

    SerializeStream ssm(ds, write);

    ssm & cd.runtimes.configurations
        & cd.runtimes.additionalLaunchers
        & cd.containers.configurations
        & cd.containers.selection
        & cd.intents.timeouts.disambiguation
        & cd.intents.timeouts.startApplication
        & cd.intents.timeouts.replyFromApplication
        & cd.intents.timeouts.replyFromSystem
        & cd.plugins.startup
        & cd.plugins.container
        & cd.logging.dlt.id
        & cd.logging.dlt.description
        & cd.logging.dlt.longMessageBehavior
        & cd.logging.rules
        & cd.logging.messagePattern
        & cd.logging.useAMConsoleLogger
        & cd.installer.caCertificates
        & cd.dbus.policies
        & cd.dbus.registrations
        & cd.quicklaunch.idleLoad
        & cd.quicklaunch.runtimesPerContainer
        & cd.quicklaunch.failedStartLimit
        & cd.quicklaunch.failedStartLimitIntervalSec
        & cd.ui.style
        & cd.ui.mainQml
        & cd.ui.resources
        & cd.ui.fullscreen
        & cd.ui.windowIcon
        & cd.ui.importPaths
        & cd.ui.pluginPaths
        & cd.ui.iconThemeName
        & cd.ui.loadDummyData
        & cd.ui.iconThemeSearchPaths
        & cd.ui.opengl.desktopProfile
        & cd.ui.opengl.esMajorVersion
        & cd.ui.opengl.esMinorVersion
        & cd.applications.builtinAppsManifestDir
        & cd.applications.installationDir
        & cd.applications.documentDir
        & cd.applications.installationDirMountPoint
        & cd.crashAction.printBacktrace
        & cd.crashAction.printQmlStack
        & cd.crashAction.waitForGdbAttach
        & cd.crashAction.dumpCore
        & cd.crashAction.stackFramesToIgnore.onCrash
        & cd.crashAction.stackFramesToIgnore.onException
        & cd.systemProperties
        & cd.flags.noSecurity
        & cd.flags.developmentMode
        & cd.flags.forceMultiProcess
        & cd.flags.forceSingleProcess
        & cd.flags.allowUnsignedPackages
        & cd.flags.allowUnknownUiClients
        & cd.wayland.socketName
              & SerializeList {
                  cd.wayland.extraSockets,
                  &ConfigurationData::Wayland::ExtraSocket::path,
                  &ConfigurationData::Wayland::ExtraSocket::permissions,
                  &ConfigurationData::Wayland::ExtraSocket::userId,
                  &ConfigurationData::Wayland::ExtraSocket::groupId
              }
        & cd.instanceId
        & cd.watchdog.disable
        & cd.watchdog.eventloop.checkInterval
        & cd.watchdog.eventloop.warnTimeout
        & cd.watchdog.eventloop.killTimeout
        & cd.watchdog.quickwindow.checkInterval
        & cd.watchdog.quickwindow.syncWarnTimeout
        & cd.watchdog.quickwindow.syncKillTimeout
        & cd.watchdog.quickwindow.renderWarnTimeout
        & cd.watchdog.quickwindow.renderKillTimeout
        & cd.watchdog.quickwindow.swapWarnTimeout
        & cd.watchdog.quickwindow.swapKillTimeout
        & cd.watchdog.wayland.checkInterval
        & cd.watchdog.wayland.warnTimeout
        & cd.watchdog.wayland.killTimeout;
}

// using templates only would be better, but we cannot get nice pointers-to-member-data for
// all elements in our sub-structs in a generic way without a lot of boilerplate code
#define MERGE_FIELD(x) mergeField(into.x, from.x, defaultValue.x)

void ConfigurationPrivate::merge(const ConfigurationData &from, ConfigurationData &into)
{
    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust both
    //            serialize() and merge() at the same time!

    static const ConfigurationData defaultValue { };

    MERGE_FIELD(runtimes.configurations);
    MERGE_FIELD(runtimes.additionalLaunchers);
    MERGE_FIELD(containers.configurations);
    MERGE_FIELD(containers.selection);

    MERGE_FIELD(intents.timeouts.disambiguation);
    MERGE_FIELD(intents.timeouts.startApplication);
    MERGE_FIELD(intents.timeouts.replyFromApplication);
    MERGE_FIELD(intents.timeouts.replyFromSystem);
    MERGE_FIELD(plugins.startup);
    MERGE_FIELD(plugins.container);
    MERGE_FIELD(logging.dlt.id);
    MERGE_FIELD(logging.dlt.description);
    MERGE_FIELD(logging.dlt.longMessageBehavior);
    MERGE_FIELD(logging.rules);
    MERGE_FIELD(logging.messagePattern);
    MERGE_FIELD(logging.useAMConsoleLogger);
    MERGE_FIELD(installer.caCertificates);
    MERGE_FIELD(dbus.policies);
    MERGE_FIELD(dbus.registrations);
    MERGE_FIELD(quicklaunch.idleLoad);
    MERGE_FIELD(quicklaunch.runtimesPerContainer);
    MERGE_FIELD(quicklaunch.failedStartLimit);
    MERGE_FIELD(quicklaunch.failedStartLimitIntervalSec);
    MERGE_FIELD(ui.style);
    MERGE_FIELD(ui.mainQml);
    MERGE_FIELD(ui.resources);
    MERGE_FIELD(ui.fullscreen);
    MERGE_FIELD(ui.windowIcon);
    MERGE_FIELD(ui.importPaths);
    MERGE_FIELD(ui.pluginPaths);
    MERGE_FIELD(ui.iconThemeName);
    MERGE_FIELD(ui.loadDummyData);
    MERGE_FIELD(ui.iconThemeSearchPaths);
    MERGE_FIELD(ui.opengl.desktopProfile);
    MERGE_FIELD(ui.opengl.esMajorVersion);
    MERGE_FIELD(ui.opengl.esMinorVersion);
    MERGE_FIELD(applications.builtinAppsManifestDir);
    MERGE_FIELD(applications.installationDir);
    MERGE_FIELD(applications.documentDir);
    MERGE_FIELD(applications.installationDirMountPoint);
    MERGE_FIELD(crashAction.printBacktrace);
    MERGE_FIELD(crashAction.printQmlStack);
    MERGE_FIELD(crashAction.waitForGdbAttach);
    MERGE_FIELD(crashAction.dumpCore);
    MERGE_FIELD(crashAction.stackFramesToIgnore.onCrash);
    MERGE_FIELD(crashAction.stackFramesToIgnore.onException);
    MERGE_FIELD(systemProperties);
    MERGE_FIELD(flags.noSecurity);
    MERGE_FIELD(flags.developmentMode);
    MERGE_FIELD(flags.forceMultiProcess);
    MERGE_FIELD(flags.forceSingleProcess);
    MERGE_FIELD(flags.allowUnsignedPackages);
    MERGE_FIELD(flags.allowUnknownUiClients);
    MERGE_FIELD(wayland.socketName);
    MERGE_FIELD(wayland.extraSockets);
    MERGE_FIELD(instanceId);
    MERGE_FIELD(watchdog.disable);
    MERGE_FIELD(watchdog.eventloop.checkInterval);
    MERGE_FIELD(watchdog.eventloop.warnTimeout);
    MERGE_FIELD(watchdog.eventloop.killTimeout);
    MERGE_FIELD(watchdog.quickwindow.checkInterval);
    MERGE_FIELD(watchdog.quickwindow.syncWarnTimeout);
    MERGE_FIELD(watchdog.quickwindow.syncKillTimeout);
    MERGE_FIELD(watchdog.quickwindow.renderWarnTimeout);
    MERGE_FIELD(watchdog.quickwindow.renderKillTimeout);
    MERGE_FIELD(watchdog.quickwindow.swapWarnTimeout);
    MERGE_FIELD(watchdog.quickwindow.swapKillTimeout);
    MERGE_FIELD(watchdog.wayland.checkInterval);
    MERGE_FIELD(watchdog.wayland.warnTimeout);
    MERGE_FIELD(watchdog.wayland.killTimeout);
}

QByteArray ConfigurationPrivate::substituteVars(const QByteArray &sourceContent, const QString &fileName)
{
    QByteArray string = sourceContent;
    QByteArray path;
    qsizetype posBeg = -1;
    qsizetype posEnd = -1;
    while (true) {
        if ((posBeg = string.indexOf("${", posEnd + 1)) < 0)
            break;
        if ((posEnd = string.indexOf('}', posBeg + 2)) < 0)
            break;

        const QByteArray varName = string.mid(posBeg + 2, posEnd - posBeg - 2);

        QByteArray varValue;
        if (varName == "CONFIG_PWD") {
            if (path.isEmpty() && !fileName.isEmpty())
                path = QFileInfo(fileName).path().toUtf8();
            varValue = path;
        } else if (varName.startsWith("env:")) {
            varValue = qgetenv(varName.constData() + 4);
        } else if (varName.startsWith("stdpath:")) {
            bool exists;
            int loc = QMetaEnum::fromType<QStandardPaths::StandardLocation>().keyToValue(varName.constData() + 8, &exists);
            if (exists)
                varValue = QStandardPaths::writableLocation(static_cast<QStandardPaths::StandardLocation>(loc)).toUtf8();
        }

        if (varValue.isNull()) {
            qCWarning(LogDeployment).nospace() << "Could not replace variable ${" << varName << "} while parsing "
                                               << fileName;
            continue;
        }
        string.replace(posBeg, varName.length() + 3, varValue);
        // varName and varValue most likely have a different length, so we have to adjust
        posEnd = posEnd - 3 - varName.length() + varValue.length();
    }
    return string;
}

void ConfigurationPrivate::loadFromSource(QIODevice *source, const QString &fileName, ConfigurationData &cd)
{
    try {
        YamlParser yp(source->readAll(), fileName);
        auto header = yp.parseHeader();
        if (!(header.first == u"am-configuration" && header.second == 1))
            throw Exception("Unsupported format type and/or version");
        yp.nextDocument();

        QString pwd;
        if (!fileName.isEmpty())
            pwd = QFileInfo(fileName).absoluteDir().path();

        yp.parseFields({
            { "instanceId", false, YamlParser::Scalar, [&]() {
                 cd.instanceId = yp.parseString(); } },
            { "runtimes", false, YamlParser::Map, [&]() {
                 cd.runtimes.configurations = yp.parseMap();
                 QVariant additionalLaunchers = cd.runtimes.configurations.take(u"additionalLaunchers"_s);
                 cd.runtimes.additionalLaunchers = variantToStringList(additionalLaunchers); } },
            { "containers", false, YamlParser::Map, [&]() {
                 cd.containers.configurations = yp.parseMap();

                 QVariant containerSelection = cd.containers.configurations.take(u"selection"_s);


                 QList<QPair<QString, QString>> config;

                 // this is easy to get wrong in the config file, so we do not just ignore a map here
                 // (this will in turn trigger the warning below)
                 if (containerSelection.metaType() == QMetaType::fromType<QVariantMap>())
                     containerSelection = QVariantList { containerSelection };

                 if (containerSelection.metaType() == QMetaType::fromType<QString>()) {
                     config.append(qMakePair(u"*"_s, containerSelection.toString()));
                 } else if (containerSelection.metaType() == QMetaType::fromType<QVariantList>()) {
                     const QVariantList list = containerSelection.toList();
                     for (const QVariant &v : list) {
                         if (v.metaType() == QMetaType::fromType<QVariantMap>()) {
                             QVariantMap map = v.toMap();

                             if (map.size() != 1) {
                                 qCWarning(LogDeployment) << "The container selection configuration needs to be a "
                                                             "list of single mappings, in order to preserve the "
                                                             "evaluation order: found a mapping with"
                                                          << map.size() << "entries.";
                             }

                             for (auto it = map.cbegin(); it != map.cend(); ++it)
                                 config.append(qMakePair(it.key(), it.value().toString()));
                         }
                     }
                 }
                 cd.containers.selection = config; } },
            { "plugins", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "startup", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.plugins.startup = yp.parseStringOrStringList(); } },
                     { "container", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.plugins.container = yp.parseStringOrStringList(); } },
                 }); } },
            { "logging", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "rules", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.logging.rules = yp.parseStringOrStringList(); } },
                     { "messagePattern", false, YamlParser::Scalar, [&]() {
                          cd.logging.messagePattern = yp.parseString(); } },
                     { "useAMConsoleLogger", false, YamlParser::Scalar, [&]() {
                          cd.logging.useAMConsoleLogger = yp.parseScalar();
                          if (cd.logging.useAMConsoleLogger.typeId() != QMetaType::Bool)
                              cd.logging.useAMConsoleLogger.clear();  } },
                     { "dlt", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "id", false, YamlParser::Scalar, [&]() {
                                   cd.logging.dlt.id = yp.parseString(); } },
                              { "description", false, YamlParser::Scalar, [&]() {
                                   cd.logging.dlt.description = yp.parseString(); } },
                              { "longMessageBehavior", false, YamlParser::Scalar, [&]() {
                                   static const QStringList validValues {
                                       u"split"_s, u"truncate"_s, u"pass"_s
                                   };
                                   QString s = yp.parseString().trimmed();
                                   if (!s.isEmpty() && !validValues.contains(s)) {
                                       throw YamlParserException(&yp, "dlt.longMessageBehavior needs to be one of %1").arg(validValues);
                                   }
                                   cd.logging.dlt.longMessageBehavior = s;
                               } }
                          }); } }
                 }); } },
            { "installer", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "disable", false, YamlParser::Scalar, [&]() {
                          qCDebug(LogDeployment) << "ignoring 'installer/disable'";
                          (void) yp.parseBool(); } },
                     { "caCertificates", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.installer.caCertificates = yp.parseStringOrStringList(); } },
                 }); } },
            { "quicklaunch", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "idleLoad", false, YamlParser::Scalar, [&]() {
                          cd.quicklaunch.idleLoad = yp.parseScalar().toDouble(); } },
                     { "runtimesPerContainer", false, YamlParser::Scalar | YamlParser::Map, [&]() {
                          // this can be set to different things:
                          //  - just a number -> the same count for any container/runtime combo (the only option pre 6.7)
                          //  - a "<container-id>": mapping -> you can map to
                          //    - just a number -> the same count for any runtime in these containers
                          //    - a "<runtime-id>": mapping -> a specific count for this container/runtime combo
                          static const QString anyId = u"*"_s;

                          auto checkRPC = [&yp](const QVariant &v) -> int {
                              if (v.typeId() == QMetaType::Int) {
                                  bool ok;
                                  int rpc = v.toInt(&ok);
                                  if (ok && (rpc >= 0) && (rpc <= 10))
                                      return rpc;
                              }
                              throw YamlParserException(&yp, "runtimesPerContainer needs to be an integer between 0 and 10");
                          };

                          if (yp.isScalar()) {
                              cd.quicklaunch.runtimesPerContainer[{ anyId, anyId }] = checkRPC(yp.parseScalar());
                          } else {
                              const QVariantMap containerIdMap = yp.parseMap();
                              for (auto cit = containerIdMap.cbegin(); cit != containerIdMap.cend(); ++cit) {
                                  const QString &cId = cit.key();
                                  const QVariant &value = cit.value();

                                  switch (value.metaType().id()) {
                                  case QMetaType::Int:
                                      cd.quicklaunch.runtimesPerContainer[{ cId, anyId }] = checkRPC(value);
                                      break;
                                  case QMetaType::QVariantMap: {
                                      const QVariantMap runtimeIdMap = value.toMap();
                                      for (auto rit = runtimeIdMap.cbegin(); rit != runtimeIdMap.cend(); ++rit) {
                                          const QString &rId = rit.key();
                                          cd.quicklaunch.runtimesPerContainer[{ cId, rId }] = checkRPC(rit.value());
                                      }
                                      break;
                                  }
                                  default:
                                      throw YamlParserException(&yp, "quicklaunch.runtimesPerContainer is invalid");
                                  }
                              }
                          } } },
                     { "failedStartLimit", false, YamlParser::Scalar, [&]() {
                          cd.quicklaunch.failedStartLimit = yp.parseInt(0); } },
                     { "failedStartLimitIntervalSec", false, YamlParser::Scalar, [&]() {
                          cd.quicklaunch.failedStartLimitIntervalSec = yp.parseDurationAsSec(u"s"); } },
                 }); } },
            { "ui", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "enableTouchEmulation", false, YamlParser::Scalar, [&]() {
                          qCDebug(LogDeployment) << "ignoring 'ui/enableTouchEmulation'";
                          (void) yp.parseScalar(); } },
                     { "iconThemeSearchPaths", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.ui.iconThemeSearchPaths = yp.parseStringOrStringList(); } },
                     { "iconThemeName", false, YamlParser::Scalar, [&]() {
                          cd.ui.iconThemeName = yp.parseString(); } },
                     { "style", false, YamlParser::Scalar, [&]() {
                          cd.ui.style = yp.parseString(); } },
                     { "loadDummyData", false, YamlParser::Scalar, [&]() {
                          cd.ui.loadDummyData = yp.parseBool(); } },
                     { "importPaths", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.ui.importPaths = yp.parseStringOrStringList(); } },
                     { "pluginPaths", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.ui.pluginPaths = yp.parseStringOrStringList(); } },
                     { "windowIcon", false, YamlParser::Scalar, [&]() {
                          cd.ui.windowIcon = yp.parseString(); } },
                     { "fullscreen", false, YamlParser::Scalar, [&]() {
                          cd.ui.fullscreen = yp.parseBool(); } },
                     { "mainQml", false, YamlParser::Scalar, [&]() {
                          cd.ui.mainQml = yp.parseString(); } },
                     { "resources", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.ui.resources = yp.parseStringOrStringList(); } },
                     { "opengl", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "desktopProfile", false, YamlParser::Scalar, [&]() {
                                   cd.ui.opengl.desktopProfile = yp.parseString(); } },
                              { "esMajorVersion", false, YamlParser::Scalar, [&]() {
                                   cd.ui.opengl.esMajorVersion = yp.parseInt(2); } },
                              { "esMinorVersion", false, YamlParser::Scalar, [&]() {
                                   cd.ui.opengl.esMinorVersion = yp.parseInt(0); } }
                          });
                      } },
                 }); } },
            { "applications", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "builtinAppsManifestDir", false, YamlParser::Scalar | YamlParser::List, [&]() {
                          cd.applications.builtinAppsManifestDir = yp.parseStringOrStringList(); } },
                     { "installationDir", false, YamlParser::Scalar | YamlParser::Scalar, [&]() {
                          cd.applications.installationDir = yp.parseString(); } },
                     { "documentDir", false, YamlParser::Scalar | YamlParser::Scalar, [&]() {
                          cd.applications.documentDir = yp.parseString(); } },
                     { "installationDirMountPoint", false, YamlParser::Scalar | YamlParser::Scalar, [&]() {
                          cd.applications.installationDirMountPoint = yp.parseString(); } },
                 }); } },
            { "flags", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "forceSingleProcess", false, YamlParser::Scalar, [&]() {
                          cd.flags.forceSingleProcess = yp.parseBool(); } },
                     { "forceMultiProcess", false, YamlParser::Scalar, [&]() {
                          cd.flags.forceMultiProcess = yp.parseBool(); } },
                     { "noSecurity", false, YamlParser::Scalar, [&]() {
                          cd.flags.noSecurity = yp.parseBool(); } },
                     { "developmentMode", false, YamlParser::Scalar, [&]() {
                          cd.flags.developmentMode = yp.parseBool(); } },
                     { "noUiWatchdog", false, YamlParser::Scalar, [&]() {
                          qCDebug(LogDeployment) << "'flags.noUiWatchdog' is deprecated, please use 'watchdog.disable'";
                          if (yp.parseBool())
                              cd.watchdog.disable = true; } },
                     { "allowUnsignedPackages", false, YamlParser::Scalar, [&]() {
                          cd.flags.allowUnsignedPackages = yp.parseBool(); } },
                     { "allowUnknownUiClients", false, YamlParser::Scalar, [&]() {
                          cd.flags.allowUnknownUiClients = yp.parseBool(); } },
                 }); } },
            { "wayland", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "socketName", false, YamlParser::Scalar, [&]() {
                          cd.wayland.socketName = yp.parseString(); } },
                     { "extraSockets", false, YamlParser::List, [&]() {
                          yp.parseList([&]() {
                              ConfigurationData::Wayland::ExtraSocket wes;
                              yp.parseFields({
                                  { "path", true, YamlParser::Scalar, [&]() {
                                       wes.path = yp.parseString(); } },
                                  { "permissions", false, YamlParser::Scalar, [&]() {
                                       wes.permissions = yp.parseInt(0); } },
                                  { "userId", false, YamlParser::Scalar, [&]() {
                                       wes.userId = yp.parseInt(); } },
                                  { "groupId", false, YamlParser::Scalar, [&]() {
                                       wes.groupId = yp.parseInt(); } }
                              });
                              cd.wayland.extraSockets.append(wes);
                          }); } }
                 }); } },
            { "systemProperties", false, YamlParser::Map, [&]() {
                 cd.systemProperties = yp.parseMap(); } },
            { "crashAction", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "printBacktrace", false, YamlParser::Scalar, [&]() {
                          cd.crashAction.printBacktrace = yp.parseBool(); } },
                     { "printQmlStack", false, YamlParser::Scalar, [&]() {
                          cd.crashAction.printQmlStack = yp.parseBool(); } },
                     { "waitForGdbAttach", false, YamlParser::Scalar, [&]() {
                          cd.crashAction.waitForGdbAttach = yp.parseDurationAsSec(u"s"); } },
                     { "dumpCore", false, YamlParser::Scalar, [&]() {
                          cd.crashAction.dumpCore = yp.parseBool(); } },
                     { "stackFramesToIgnore", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "onCrash", false, YamlParser::Scalar, [&]() {
                                   cd.crashAction.stackFramesToIgnore.onCrash = yp.parseInt(-1); } },
                              { "onException", false, YamlParser::Scalar, [&]() {
                                   cd.crashAction.stackFramesToIgnore.onException = yp.parseInt(-1); } },
                          }); } }
                 }); } },
            { "intents", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "disable", false, YamlParser::Scalar, [&]() {
                          qCDebug(LogDeployment) << "ignoring 'intents/disable'";
                          (void) yp.parseBool(); } },
                     { "timeouts", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "disambiguation", false, YamlParser::Scalar, [&]() {
                                   cd.intents.timeouts.disambiguation = yp.parseDurationAsMSec(u"ms"); } },
                              { "startApplication", false, YamlParser::Scalar, [&]() {
                                   cd.intents.timeouts.startApplication = yp.parseDurationAsMSec(u"ms"); } },
                              { "replyFromApplication", false, YamlParser::Scalar, [&]() {
                                   cd.intents.timeouts.replyFromApplication = yp.parseDurationAsMSec(u"ms"); } },
                              { "replyFromSystem", false, YamlParser::Scalar, [&]() {
                                   cd.intents.timeouts.replyFromSystem = yp.parseDurationAsMSec(u"ms"); } },
                          }); } }
                 }); } },
            { "dbus", false, YamlParser::Map, [&]() {
                 const QVariantMap dbus = yp.parseMap();
                 for (auto it = dbus.cbegin(); it != dbus.cend(); ++it) {
                     const QString &ifaceName = it.key();
                     const QVariantMap &ifaceData = it.value().toMap();

                     auto rit = ifaceData.constFind(u"register"_s);
                     if (rit != ifaceData.cend())
                         cd.dbus.registrations.insert(ifaceName, rit->toString());

                     auto pit = ifaceData.constFind(u"policy"_s);
                     if (pit != ifaceData.cend())
                         cd.dbus.policies.insert(ifaceName, pit->toMap());
                 } } },
            { "watchdog", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "disable", false, YamlParser::Scalar, [&]() {
                          cd.watchdog.disable = yp.parseBool(); } },
                     { "eventloop", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "checkInterval", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.eventloop.checkInterval = yp.parseDurationAsMSec(); } },
                              { "warnTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.eventloop.warnTimeout = yp.parseDurationAsMSec(); } },
                              { "killTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.eventloop.killTimeout = yp.parseDurationAsMSec(); } },
                          }); } },
                     { "quickwindow", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "checkInterval", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.checkInterval = yp.parseDurationAsMSec(); } },
                              { "syncWarnTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.syncWarnTimeout = yp.parseDurationAsMSec(); } },
                              { "syncKillTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.syncKillTimeout = yp.parseDurationAsMSec(); } },
                              { "renderWarnTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.renderWarnTimeout = yp.parseDurationAsMSec(); } },
                              { "renderKillTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.renderKillTimeout = yp.parseDurationAsMSec(); } },
                              { "swapWarnTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.swapWarnTimeout = yp.parseDurationAsMSec(); } },
                              { "swapKillTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.quickwindow.swapKillTimeout = yp.parseDurationAsMSec(); } },
                          }); } },
                     { "wayland", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "checkInterval", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.wayland.checkInterval = yp.parseDurationAsMSec(); } },
                              { "warnTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.wayland.warnTimeout = yp.parseDurationAsMSec(); } },
                              { "killTimeout", false, YamlParser::Scalar, [&]() {
                                   cd.watchdog.wayland.killTimeout = yp.parseDurationAsMSec(); } },
                          }); } }
                 }); } }

        });
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse config file %1: %2")
            .arg(!fileName.isEmpty() ? QDir().relativeFilePath(fileName) : u"<stream>"_s, e.errorString());
    }
}

bool Configuration::noCache() const
{
    return d->clp.isSet(u"no-cache"_s);
}

bool Configuration::clearCache() const
{
    return d->clp.isSet(u"clear-cache"_s);
}

bool Configuration::verbose() const
{
    return d->clp.isSet(u"verbose"_s) || d->forceVerbose;
}

void Configuration::setForceVerbose(bool forceVerbose)
{
    d->forceVerbose = forceVerbose;
}

void Configuration::setForceWatchdog(bool forceWatchdog)
{
    d->data.watchdog.disable = !forceWatchdog;
}

bool Configuration::slowAnimations() const
{
    return d->clp.isSet(u"slow-animations"_s);
}

bool Configuration::noDltLogging() const
{
    return d->clp.isSet(u"no-dlt-logging"_s);
}

bool Configuration::qmlDebugging() const
{
    return d->clp.isSet(u"qml-debug"_s);
}

QString Configuration::singleApp() const
{
    //TODO: single-package
    return d->clp.value(u"single-app"_s);
}

QString Configuration::dbus() const
{
    return d->clp.value(u"dbus"_s);
}

QString Configuration::waylandSocketName() const
{
    return d->clp.value(u"wayland-socket-name"_s);
}

QStringList Configuration::testRunnerArguments() const
{
    QStringList targs = d->clp.positionalArguments();
    if (!targs.isEmpty() && targs.constFirst().endsWith(u".qml"))
        targs.removeFirst();
    targs.prepend(QCoreApplication::arguments().constFirst());
    return targs;
}

QString Configuration::testRunnerSourceFile() const
{
    return d->clp.value(u"qmltestrunner-source-file"_s);
}

QT_END_NAMESPACE_AM
