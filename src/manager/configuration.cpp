/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/
#include <QCommandLineParser>
#include <QFile>
#include <QVariantMap>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDataStream>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>
#include <private/qvariant_p.h>

#include <functional>

#include "global.h"
#include "logging.h"
#include "qtyaml.h"
#include "utilities.h"
#include "exception.h"
#include "qml-utilities.h"
#include "configuration.h"

#if !defined(AM_CONFIG_FILE)
#  define AM_CONFIG_FILE "/opt/am/config.yaml"
#endif

// enable this to benchmark the config cache
//#define AM_TIME_CONFIG_PARSING

// use QtConcurrent to parse the config files, if there are more than x config files
#define AM_PARALLEL_THRESHOLD  1

QT_BEGIN_NAMESPACE_AM

class ConfigurationPrivate
{
public:
    QVariant findInConfigFile(const QStringList &path, bool *found = nullptr);

    template <typename T> T config(const char *clname, const QStringList &cfname)
    {
        Q_UNUSED(clname)
        Q_UNUSED(cfname)
        return T();
    }

    void mergeConfig(const QVariantMap &other);

    QCommandLineParser clp;
    QString mainQmlFile;
    QVariantMap configFile;
};

template<> bool ConfigurationPrivate::config(const char *clname, const QStringList &cfname)
{
    return clp.isSet(qL1S(clname)) || findInConfigFile(cfname).toBool();
}

template<> QString ConfigurationPrivate::config(const char *clname, const QStringList &cfname)
{
    QString clval = clp.value(qL1S(clname));
    bool cffound;
    QString cfval = findInConfigFile(cfname, &cffound).toString();
    return (clp.isSet(qL1S(clname)) || !cffound) ? clval : cfval;
}

template<> QStringList ConfigurationPrivate::config(const char *clname, const QStringList &cfname)
{
    return clp.values(qL1S(clname)) + variantToStringList(findInConfigFile(cfname));
}

QVariant ConfigurationPrivate::findInConfigFile(const QStringList &path, bool *found)
{
    if (found)
        *found = false;

    if (path.isEmpty())
        return QVariant();

    QVariantMap var = configFile;

    for (int i = 0; i < (path.size() - 1); ++i) {
        QVariant subvar = var.value(path.at(i));
        if (subvar.type() == QVariant::Map)
            var = subvar.toMap();
        else
            return QVariant();
    }
    if (found)
        *found = var.contains(path.last());
    return var.value(path.last());
}

void ConfigurationPrivate::mergeConfig(const QVariantMap &other)
{
    // no auto allowed, since this is a recursive lambda
    std::function<void(QVariantMap *, const QVariantMap &)> recursiveMergeMap =
            [&recursiveMergeMap](QVariantMap *to, const QVariantMap &from) {
        for (auto it = from.constBegin(); it != from.constEnd(); ++it) {
            QVariant fromValue = it.value();
            QVariant &toValue = (*to)[it.key()];

            bool needsMerge = (toValue.type() == fromValue.type());

            // we're trying not to detach, so we're using v_cast to avoid copies
            if (needsMerge && (toValue.type() == QVariant::Map))
                recursiveMergeMap(v_cast<QVariantMap>(&toValue.data_ptr()), fromValue.toMap());
            else if (needsMerge && (toValue.type() == QVariant::List))
                to->insert(it.key(), toValue.toList() + fromValue.toList());
            else
                to->insert(it.key(), fromValue);
        }
    };
    recursiveMergeMap(&configFile, other);
}


Configuration::Configuration()
    : d(new ConfigurationPrivate())
{
    // using QStringLiteral for all strings here adds a few KB of ro-data, but will also improve
    // startup times slightly: less allocations and copies. MSVC cannot cope with multi-line though

    const char *description =
#ifdef AM_TESTRUNNER
        "Pelagicore ApplicationManager QML Test Runner"
        "\n\n"
        "Additional testrunner commandline options can be set after the -- argument\n"
        "Use -- -help to show all available testrunner commandline options"
#else
        "Pelagicore ApplicationManager"
#endif
        "\n\n"
        "In addition to the commandline options below, the following environment\n"
        "variables can be set:\n\n"
        "  AM_STARTUP_TIMER  if set to 1, a startup performance analysis will be printed\n"
        "                    on the console. Anything other than 1 will be interpreted\n"
        "                    as the name of a file that is used instead of the console\n"
        "\n"
        "  AM_FORCE_COLOR_OUTPUT  can be set to 'on' to force color output to the console\n"
        "                         and to 'off' to disable it. Any other value will result\n"
        "                         in the default, auto-detection behavior.\n"
        "\n"
        "  AM_TIMEOUT_FACTOR  all timed wait statements within the application-manager will\n"
        "                     be slowed down by this (integer) factor. Useful if executing\n"
        "                     in slow wrappers, like e.g. valgrind. Defaults to 1.\n";
    d->clp.setApplicationDescription(description);
    d->clp.addHelpOption();
    d->clp.addVersionOption();

    d->clp.addPositionalArgument(qSL("qml-file"),   qSL("the main QML file."));
    d->clp.addOption({ { qSL("c"), qSL("config-file") }, qSL("load configuration from file (can be given multiple times)."), qSL("files"), qSL(AM_CONFIG_FILE) });
    d->clp.addOption({ qSL("database"),             qSL("application database."), qSL("file"), qSL("/opt/am/apps.db") });
    d->clp.addOption({ { qSL("r"), qSL("recreate-database") },  qSL("recreate the application database.") });
    d->clp.addOption({ qSL("builtin-apps-manifest-dir"),   qSL("base directory for built-in application manifests."), qSL("dir") });
    d->clp.addOption({ qSL("installed-apps-manifest-dir"), qSL("base directory for installed application manifests."), qSL("dir"), qSL("/opt/am/manifests") });
    d->clp.addOption({ qSL("app-image-mount-dir"),  qSL("base directory where application images are mounted to."), qSL("dir"), qSL("/opt/am/image-mounts") });
#if defined(QT_DBUS_LIB)
#  if defined(Q_OS_LINUX)
    const QString defaultDBus = qSL("session");
#  else
    const QString defaultDBus = qSL("none");
#  endif
    d->clp.addOption({ qSL("dbus"),                 qSL("register on the specified D-Bus."), qSL("<bus>|system|session|none"), defaultDBus });
    d->clp.addOption({ qSL("start-session-dbus"),   qSL("start a private session bus instead of using an existing one.") });
#endif
    d->clp.addOption({ qSL("fullscreen"),           qSL("display in full-screen.") });
    d->clp.addOption({ qSL("no-fullscreen"),        qSL("do not display in full-screen.") });
    d->clp.addOption({ qSL("I"),                    qSL("additional QML import path."), qSL("dir") });
    d->clp.addOption({ qSL("verbose"),              qSL("verbose output.") });
    d->clp.addOption({ qSL("slow-animations"),      qSL("run all animations in slow motion.") });
    d->clp.addOption({ qSL("load-dummydata"),       qSL("loads QML dummy-data.") });
    d->clp.addOption({ qSL("no-security"),          qSL("disables all security related checks (dev only!)") });
    d->clp.addOption({ qSL("no-ui-watchdog"),       qSL("disables detecting hung UI applications (e.g. via Wayland's ping/pong).") });
    d->clp.addOption({ qSL("no-dlt-logging"),       qSL("disables logging using automotive DLT.") });
    d->clp.addOption({ qSL("force-single-process"), qSL("forces single-process mode even on a wayland enabled build.") });
    d->clp.addOption({ qSL("force-multi-process"),  qSL("forces multi-process mode. Will exit immediately if this is not possible.") });
    d->clp.addOption({ qSL("wayland-socket-name"),  qSL("use this file name to create the wayland socket."), qSL("socket") });
    d->clp.addOption({ qSL("single-app"),           qSL("runs a single application only (ignores the database)"), qSL("info.yaml file") });
    d->clp.addOption({ qSL("logging-rule"),         qSL("adds a standard Qt logging rule."), qSL("rule") });
    d->clp.addOption({ qSL("build-config"),         qSL("dumps the build configuration and exits.") });
    d->clp.addOption({ qSL("no-config-cache"),      qSL("disable the use of the config file cache.") });
    d->clp.addOption({ qSL("clear-config-cache"),   qSL("ignore an existing config file cache.") });
    d->clp.addOption({ qSL("qml-debug"),            qSL("enables QML debugging and profiling.") });
    d->clp.addOption({ { qSL("o"), qSL("option") }, qSL("override a specific config option."), qSL("yaml-snippet") });
}

Configuration::~Configuration()
{
    delete d;
}

// vvvv copied from QCommandLineParser ... why is this not public API?

#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
#  include <windows.h>
#elif defined(Q_OS_ANDROID)
#  include <android/log.h>
#endif

enum MessageType { UsageMessage, ErrorMessage };

#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
// Return whether to use a message box. Use handles if a console can be obtained
// or we are run with redirected handles (for example, by QProcess).
static inline bool displayMessageBox()
{
    if (GetConsoleWindow())
        return false;
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&startupInfo);
    return !(startupInfo.dwFlags & STARTF_USESTDHANDLES);
}
#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT

static void showParserMessage(const QString &message, MessageType type)
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
    if (displayMessageBox()) {
        const UINT flags = MB_OK | MB_TOPMOST | MB_SETFOREGROUND
            | (type == UsageMessage ? MB_ICONINFORMATION : MB_ICONERROR);
        QString title;
        if (QCoreApplication::instance())
            title = QCoreApplication::instance()->property("applicationDisplayName").toString();
        if (title.isEmpty())
            title = QCoreApplication::applicationName();
        MessageBoxW(0, reinterpret_cast<const wchar_t *>(message.utf16()),
                    reinterpret_cast<const wchar_t *>(title.utf16()), flags);
        return;
    }
#elif defined(Q_OS_ANDROID)
    static QByteArray appName = QCoreApplication::applicationName().toLocal8Bit();

    __android_log_print(type == UsageMessage ? ANDROID_LOG_WARN : ANDROID_LOG_ERROR,
                        appName.constData(), qPrintable(message));
    return;

#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT
    fputs(qPrintable(message), type == UsageMessage ? stdout : stderr);
}

// ^^^^ copied from QCommandLineParser ... why is this not public API?


void Configuration::parse()
{
    if (!d->clp.parse(QCoreApplication::arguments())) {
        showParserMessage(d->clp.errorText() + qL1C('\n'), ErrorMessage);
        exit(1);
    }

    if (d->clp.isSet(qSL("version")))
        d->clp.showVersion();

    if (d->clp.isSet(qSL("help")))
        d->clp.showHelp();

    if (d->clp.isSet(qSL("build-config"))) {
        QFile f(qSL(":/build-config.yaml"));
        if (f.open(QFile::ReadOnly)) {
            showParserMessage(QString::fromLocal8Bit(f.readAll()), UsageMessage);
            exit(0);
        } else {
            showParserMessage(qL1S("Could not find the embedded build config.\n"), ErrorMessage);
            exit(1);
        }
    }

#ifndef AM_TESTRUNNER
    if (d->clp.positionalArguments().size() > 1) {
        showParserMessage(qL1S("Only one main qml file can be specified.\n"), ErrorMessage);
        exit(1);
    }
#endif

#if defined(AM_TIME_CONFIG_PARSING)
    QElapsedTimer timer;
    timer.start();
#endif

    QStringList configFilePaths = d->clp.values(qSL("config-file"));

    struct ConfigFile
    {
        QString filePath;    // abs. file path
        QByteArray checksum; // sha1 (fast and sufficient for this use-case)
        QByteArray content;
        QVariantMap config;
    };
    QVarLengthArray<ConfigFile> configFiles(configFilePaths.size());

    for (int i = 0; i < configFiles.size(); ++i)
        configFiles[i].filePath = QFileInfo(configFilePaths.at(i)).absoluteFilePath();

    bool noConfigCache = d->clp.isSet(qSL("no-config-cache"));
    bool clearConfigCache = d->clp.isSet(qSL("clear-config-cache"));

    const QDir cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (!cacheLocation.exists())
        cacheLocation.mkpath(qSL("."));

    const QString cacheFilePath = cacheLocation.absoluteFilePath(qSL("appman-config.cache"));

    QFile cacheFile(cacheFilePath);
    QAtomicInt useCache = false;
    QVariantMap cache;

    if (!noConfigCache && !clearConfigCache) {
        if (cacheFile.open(QFile::ReadOnly)) {
            try {
                QDataStream ds(&cacheFile);
                QVector<QPair<QString, QByteArray>> configChecksums; // abs. file path -> sha1
                ds >> configChecksums >> cache;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("failed to read config cache content");

                if (configFiles.count() != configChecksums.count())
                    throw Exception("the number of cached config files does not match the current set");

                for (int i = 0; i < configFiles.count(); ++i) {
                    ConfigFile &cf = configFiles[i];
                    if (cf.filePath != configChecksums.at(i).first)
                        throw Exception("the cached config file names do not match the current set (or their order changed)");
                    cf.checksum = configChecksums.at(i).second;
                }
                useCache = true;

#if defined(AM_TIME_CONFIG_PARSING)
                qCDebug(LogSystem) << "Config parsing: cache loaded after" << (timer.nsecsElapsed() / 1000) << "usec";
#endif
            } catch (const Exception &e) {
                qCWarning(LogSystem) << "Failed to read config cache:" << e.what();
            }
        }
    }

    // reads a single config file and calculates its hash - defined as lambda to be usable
    // both via QtConcurrent and via std:for_each
    auto readConfigFile = [&useCache](ConfigFile &cf) {
        QFile file(cf.filePath);
        if (!file.open(QIODevice::ReadOnly))
            throw Exception("Failed to open config file '%1' for reading.\n").arg(file.fileName());

        if (file.size() > 1024*1024)
            throw Exception("Config file '%1' is too big (> 1MB).\n").arg(file.fileName());

        cf.content = file.readAll();

        QByteArray checksum = QCryptographicHash::hash(cf.content, QCryptographicHash::Sha1);
        if (useCache && (checksum != cf.checksum)) {
            qCWarning(LogSystem) << "Failed to read config cache: cached config file checksums do not match current set";
            useCache = false;
        }
        cf.checksum = checksum;
    };

    try {
        if (configFiles.size() > AM_PARALLEL_THRESHOLD)
            QtConcurrent::blockingMap(configFiles, readConfigFile);
        else
            std::for_each(configFiles.begin(), configFiles.end(), readConfigFile);
    } catch (const Exception &e) {
        showParserMessage(e.errorString(), ErrorMessage);
        exit(1);
    }

#if defined(AM_TIME_CONFIG_PARSING)
    qCDebug(LogSystem) << "Config parsing" << configFiles.size() << "files: loading finished after"
                       << (timer.nsecsElapsed() / 1000) << "usec";
#endif

    if (useCache) {
        qCDebug(LogSystem) << "Using existing config cache:" << cacheFilePath;

        d->configFile = cache;
    } else if (!configFilePaths.isEmpty()) {
        auto parseConfigFile = [](ConfigFile &cf) {
            // we want to replace ${CONFIG_PWD} (when at the start of a value) with the abs. path
            // to the config file it appears in, similar to qmake's $$_PRO_FILE_PWD
            QString configFilePath = QFileInfo(cf.filePath).absolutePath();

            auto replaceConfigPwd = [configFilePath](const QVariant &value) -> QVariant {
                if (value.type() == QVariant::String) {
                    QString str = value.toString();
                    if (str.startsWith(qSL("${CONFIG_PWD}")))
                        return QVariant(configFilePath + str.midRef(13));
                }
                return value;
            };

            QtYaml::ParseError parseError;
            QVector<QVariant> docs = QtYaml::variantDocumentsFromYamlFiltered(cf.content, replaceConfigPwd, &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                throw Exception("Could not parse config file '%1', line %2, column %3: %4.\n")
                        .arg(cf.filePath).arg(parseError.line).arg(parseError.column)
                        .arg(parseError.errorString());
            }

            try {
                checkYamlFormat(docs, 2 /*number of expected docs*/, { "am-configuration" }, 1);
            } catch (const Exception &e) {
                throw Exception("Could not parse config file '%1': %2.\n")
                        .arg(cf.filePath).arg(e.errorString());
            }
            cf.config = docs.at(1).toMap();
        };

        try {
            if (configFiles.size() > AM_PARALLEL_THRESHOLD)
                QtConcurrent::blockingMap(configFiles, parseConfigFile);
            else
                std::for_each(configFiles.begin(), configFiles.end(), parseConfigFile);
        } catch (const Exception &e) {
            showParserMessage(e.errorString(), ErrorMessage);
            exit(1);
        }

        // we cannot parallelize this step, since subsequent config files can overwrite
        // or append to values
        d->configFile = configFiles.at(0).config;
        for (int i = 1; i < configFiles.size(); ++i)
            d->mergeConfig(configFiles.at(i).config);

        if (!noConfigCache) {
            try {
                QFile cacheFile(cacheFilePath);
                if (!cacheFile.open(QFile::WriteOnly | QFile::Truncate))
                    throw Exception(cacheFile, "failed to open file for writing");

                QDataStream ds(&cacheFile);
                QVector<QPair<QString, QByteArray>> configChecksums;
                for (const ConfigFile &cf : qAsConst(configFiles))
                    configChecksums.append(qMakePair(cf.filePath, cf.checksum));

                ds << configChecksums << d->configFile;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("error writing config cache content");
            } catch (const Exception &e) {
                qCWarning(LogSystem) << "Failed to write config cache:" << e.what();
            }
        }
#if defined(AM_TIME_CONFIG_PARSING)
        qCDebug(LogSystem) << "Config parsing" << configFiles.size() << "files: parsing finished after"
                           << (timer.nsecsElapsed() / 1000) << "usec";
#endif
    }

    const QStringList options = d->clp.values(qSL("o"));
    for (const QString &option : options) {
        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(option.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            showParserMessage(QString::fromLatin1("Could not parse --option value, column %1: %2.\n")
                              .arg(parseError.column).arg(parseError.errorString()),
                              ErrorMessage);
            exit(1);
        }
        if (docs.size() != 1) {
            showParserMessage(QString::fromLatin1("Could not parse --option value: Invalid document format.\n"),
                              ErrorMessage);
            exit(1);
        }
        d->mergeConfig(docs.at(0).toMap());
    }

#if defined(AM_TIME_CONFIG_PARSING)
    qCDebug(LogSystem) << "Config parsing" << options.size() << "-o options: parsing finished after"
                       << (timer.nsecsElapsed() / 1000) << "usec";
#endif

    // QML cannot cope with invalid QVariants and QDataStream cannot cope with nullptr inside a
    // QVariant ... the workaround is to save invalid variants to the cache and fix them up
    // afterwards:
    fixNullValuesForQml(d->configFile);
}

QString Configuration::mainQmlFile() const
{
    if (!d->clp.positionalArguments().isEmpty() && d->clp.positionalArguments().at(0).endsWith(qL1S(".qml")))
        return d->clp.positionalArguments().at(0);
    else
        return d->findInConfigFile({ qSL("ui"), qSL("mainQml") }).toString();
}


QString Configuration::database() const
{
    return d->config<QString>("database", { qSL("applications"), qSL("database") });
}

bool Configuration::recreateDatabase() const
{
    return d->clp.isSet(qSL("recreate-database"));
}

QStringList Configuration::builtinAppsManifestDirs() const
{
    return d->config<QStringList>("builtin-apps-manifest-dir", { qSL("applications"), qSL("builtinAppsManifestDir") });
}

QString Configuration::installedAppsManifestDir() const
{
    return d->config<QString>("installed-apps-manifest-dir", { qSL("applications"), qSL("installedAppsManifestDir") });
}

QString Configuration::appImageMountDir() const
{
    return d->config<QString>("app-image-mount-dir", { qSL("applications"), qSL("appImageMountDir") });
}

bool Configuration::fullscreen() const
{
    return d->config<bool>("fullscreen", { qSL("ui"), qSL("fullscreen") });
}

bool Configuration::noFullscreen() const
{
    return d->clp.isSet(qSL("no-fullscreen"));
}

QString Configuration::windowIcon() const
{
    return d->findInConfigFile({ qSL("ui"), qSL("windowIcon") }).toString();
}

QStringList Configuration::importPaths() const
{
    QStringList importPaths = d->config<QStringList>("I", { qSL("ui"), qSL("importPaths") });

    for (int i = 0; i < importPaths.size(); ++i)
        importPaths[i] = QFileInfo(importPaths.at(i)).absoluteFilePath();

    return importPaths;
}

bool Configuration::verbose() const
{
    return d->clp.isSet(qSL("verbose"));
}

bool Configuration::slowAnimations() const
{
    return d->clp.isSet(qSL("slow-animations"));
}

bool Configuration::loadDummyData() const
{
    return d->config<bool>("load-dummydata", { qSL("ui"), qSL("loadDummyData") });
}

bool Configuration::noSecurity() const
{
    return d->config<bool>("no-security", { qSL("flags"), qSL("noSecurity") });
}

bool Configuration::noUiWatchdog() const
{
    return d->config<bool>("no-ui-watchdog", { qSL("flags"), qSL("noUiWatchdog") });
}

bool Configuration::noDltLogging() const
{
    return d->clp.isSet(qSL("no-dlt-logging"));
}

bool Configuration::forceSingleProcess() const
{
    return d->config<bool>("force-single-process", { qSL("flags"), qSL("forceSingleProcess") });
}

bool Configuration::forceMultiProcess() const
{
    return d->config<bool>("force-multi-process", { qSL("flags"), qSL("forceMultiProcess") });
}

bool Configuration::qmlDebugging() const
{
    return d->clp.isSet(qSL("qml-debug"));
}

QString Configuration::singleApp() const
{
    return d->clp.value(qSL("single-app"));
}

QStringList Configuration::loggingRules() const
{
    return d->config<QStringList>("logging-rule", { qSL("logging"), qSL("rules") });
}

QString Configuration::style() const
{
    return d->findInConfigFile({ qSL("ui"), qSL("style") }).toString();
}

QVariantList Configuration::installationLocations() const
{
    return d->findInConfigFile({ qSL("installationLocations") }).toList();
}

QList<QPair<QString, QString>> Configuration::containerSelectionConfiguration() const
{
    QList<QPair<QString, QString>> config;
    QVariant containerSelection = d->findInConfigFile({ qSL("containers"), qSL("selection") });

    // this is easy to get wrong in the config file, so we do not just ignore a map here
    // (this will in turn trigger the warning below)
    if (containerSelection.type() == QVariant::Map)
        containerSelection = QVariantList { containerSelection };

    if (containerSelection.type() == QVariant::String) {
        config.append(qMakePair(qSL("*"), containerSelection.toString()));
    } else if (containerSelection.type() == QVariant::List) {
        QVariantList list = containerSelection.toList();
        for (const QVariant &v : list) {
            if (v.type() == QVariant::Map) {
                QVariantMap map = v.toMap();

                if (map.size() != 1) {
                    qCWarning(LogSystem) << "The container selection configuration needs to be a list of "
                                            "single mappings, in order to preserve the evaluation "
                                            "order: found a mapping with" << map.size() << "entries.";
                }

                for (auto it = map.cbegin(); it != map.cend(); ++it)
                    config.append(qMakePair(it.key(), it.value().toString()));
            }
        }
    }
    return config;
}

QVariantMap Configuration::containerConfigurations() const
{
    QVariantMap map = d->findInConfigFile({ qSL("containers") }).toMap();
    map.remove(qSL("selection"));
    return map;
}

QVariantMap Configuration::runtimeConfigurations() const
{
    return d->findInConfigFile({ qSL("runtimes") }).toMap();
}

QVariantMap Configuration::dbusPolicy(const QString &interfaceName) const
{
    return d->findInConfigFile({ qSL("dbus"), interfaceName, qSL("policy") }).toMap();
}

QString Configuration::dbusRegistration(const QString &interfaceName) const
{
    QString dbus = d->config<QString>("dbus", { qSL("dbus"), interfaceName, qSL("register") });
    if (dbus == qL1S("none"))
        dbus.clear();
    return dbus;
}

int Configuration::dbusRegistrationDelay() const
{
    bool found = false;
    int delay = d->findInConfigFile({ qSL("dbus"), qSL("registrationDelay") }, &found).toInt();
    return found ? delay : -1;
}

bool Configuration::dbusStartSessionBus() const
{
    return d->config<bool>("start-session-dbus", { qSL("dbus"), qSL("startSessionBus") });
}

QVariantMap Configuration::rawSystemProperties() const
{
    return d->findInConfigFile({ qSL("systemProperties") }).toMap();
}

bool Configuration::applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const
{
    bool found = false;
    QVariantMap map = d->findInConfigFile({ qSL("installer"), qSL("applicationUserIdSeparation") }, &found).toMap();

    if (found) {
        auto idFromMap = [&map](const char *key) -> uint {
            bool ok;
            uint value = map.value(qL1S(key)).toUInt(&ok);
            return ok ? value : uint(-1);
        };

        uint undef = uint(-1);
        uint minUser = idFromMap("minUserId");
        uint maxUser = idFromMap("maxUserId");
        uint commonGroup = idFromMap("commonGroupId");

        if (minUser != undef && maxUser != undef && commonGroup != undef && minUser < maxUser) {
            if (minUserId)
                *minUserId = minUser;
            if (maxUserId)
                *maxUserId = maxUser;
            if (commonGroupId)
                *commonGroupId = commonGroup;
            return true;
        }
    }
    return false;
}

qreal Configuration::quickLaunchIdleLoad() const
{
    bool found, conversionOk;
    qreal idleLoad = d->findInConfigFile({ qSL("quicklaunch"), qSL("idleLoad") }, &found).toReal(&conversionOk);
    return (found && conversionOk) ? idleLoad : qreal(0);
}

int Configuration::quickLaunchRuntimesPerContainer() const
{
    bool found, conversionOk;
    int rpc = d->findInConfigFile({ qSL("quicklaunch"), qSL("runtimesPerContainer") }, &found).toInt(&conversionOk);

    // if you need more than 10 quicklaunchers per runtime, you're probably doing something wrong
    // or you have a typo in your YAML, which could potentially freeze your target (container
    // construction can be expensive)
    return (found && conversionOk && rpc >= 0 && rpc < 10) ? rpc : 0;
}

QString Configuration::waylandSocketName() const
{
    return d->clp.value(qSL("wayland-socket-name"));
}

QString Configuration::telnetAddress() const
{
    QString s = d->findInConfigFile({ qSL("debug"), qSL("telnetAddress") }, nullptr).toString();
    if (s.isEmpty())
        s = qSL("0.0.0.0");
    return s;
}

quint16 Configuration::telnetPort() const
{
    return d->findInConfigFile({ qSL("debug"), qSL("telnetPort") }, nullptr).value<quint16>();
}

QVariantList Configuration::debugWrappers() const
{
    return d->findInConfigFile({ qSL("debugWrappers") }).toList();
}

QVariantMap Configuration::managerCrashAction() const
{
    return d->findInConfigFile({ qSL("crashAction")} ).toMap();
}

QStringList Configuration::caCertificates() const
{
    return variantToStringList(d->findInConfigFile({ qSL("installer"), qSL("caCertificates") }));
}

QStringList Configuration::pluginFilePaths(const char *type) const
{
    return variantToStringList(d->findInConfigFile({ qSL("plugins"), qL1S(type) }));
}

QStringList Configuration::testRunnerArguments() const
{
    QStringList targs = d->clp.positionalArguments();
    if (!targs.isEmpty() && targs.constFirst().endsWith(qL1S(".qml")))
        targs.removeFirst();
    targs.prepend(QCoreApplication::arguments().constFirst());
    return targs;
}

QT_END_NAMESPACE_AM
