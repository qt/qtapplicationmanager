/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
#include <QDebug>

#include <functional>

#include "global.h"
#include "qtyaml.h"
#include "configuration.h"

#if !defined(AM_CONFIG_FILE)
#  define AM_CONFIG_FILE "/opt/am/config.yaml"
#endif


class ConfigurationPrivate
{
public:
    QVariant findInConfigFile(const QStringList &path, bool *found = 0);

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
    return clp.values(qL1S(clname)) + findInConfigFile(cfname).toStringList();
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
    std::function<void(QVariantMap &, const QVariantMap &)> recursiveMergeMap =
            [&recursiveMergeMap](QVariantMap &to, const QVariantMap &from) {
        for (auto it = from.constBegin(); it != from.constEnd(); ++it) {
            QVariant fromValue = it.value();
            QVariant toValue = to.value(it.key());

            bool needsMerge = (toValue.type() == fromValue.type());

            if (needsMerge && (toValue.type() == QVariant::Map)) {
                QVariantMap tmpMap = toValue.toMap();
                recursiveMergeMap(tmpMap, fromValue.toMap());
                to.insert(it.key(), tmpMap);
            } else if (needsMerge && (toValue.type() == QVariant::List)) {
                to.insert(it.key(), toValue.toList() + fromValue.toList());
            } else {
                to.insert(it.key(), fromValue);
            }
        }
    };
    recursiveMergeMap(configFile, other);
}


Configuration::Configuration()
    : d(new ConfigurationPrivate())
{
    // using QStringLiteral for all strings here adds a few KB of ro-data, but will also improve
    // startup times slightly: less allocations and copies. MSVC cannot cope with multi-line though

    d->clp.setApplicationDescription(qL1S("Pelagicore ApplicationManager"
                                          "\n\n"
                                          "In addition to the commandline options below, the following environment\n"
                                          "variables can be set:\n\n"
                                          "  AM_FAKE_SUDO      if set to 1, no root privileges will be acquired\n"
                                          "  AM_STARTUP_TIMER  if set to 1, a startup performance analysis will be printed\n"
                                          "                    on the console. Anything other than 1 will interpreted\n"
                                          "                    as the name of a file that is used instead of the console\n"));
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
    d->clp.addOption({ qSL("dbus"),                 qSL("register on the specified D-Bus."), qSL("<bus>|system|session|none"), qSL("session") });
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
    d->clp.addOption({ qSL("force-single-process"), qSL("forces single-process mode even on a wayland enabled build.") });
    d->clp.addOption({ qSL("wayland-socket-name"),  qSL("use this file name to create the wayland socket."), qSL("socket"), qSL("wayland-0") });
    d->clp.addOption({ qSL("single-app"),           qSL("runs a single application only (ignores the database)"), qSL("info.yaml file") });
    d->clp.addOption({ qSL("logging-rule"),         qSL("adds a standard Qt logging rule."), qSL("rule") });
    d->clp.addOption({ qSL("build-config"),         qSL("dumps the build configuration and exits.") });

    initialize();
}

// vvvv copied from QCommandLineParser ... why is this not public API?

#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
# include <windows.h>
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
#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT
    fputs(qPrintable(message), type == UsageMessage ? stdout : stderr);
}

// ^^^^ copied from QCommandLineParser ... why is this not public API?


void Configuration::initialize()
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

    if (d->clp.positionalArguments().size() > 1) {
        showParserMessage(qL1S("Only one main qml file can be specified.\n"), ErrorMessage);
        exit(1);
    }

    QStringList configFilePaths = d->clp.values(qSL("config-file"));
#if defined(Q_OS_ANDROID)
    if (!d->clp.isSet(qSL("config-file"))) {
        if (!(QString sdFilePath = findOnSDCard(qSL("application-manager.conf"))).isEmpty())
            configFilePaths = QStringList(sdFilePath);
    }
#endif

    foreach (const QString &configFilePath, configFilePaths) {
        QFile cf(configFilePath);
        if (!cf.open(QIODevice::ReadOnly)) {
            showParserMessage(QString::fromLatin1("Failed to open config file '%1' for reading.\n").arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        if (cf.size() > 1024*1024) {
            showParserMessage(QString::fromLatin1("Config file '%1' is too big (> 1MB).\n").arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(cf.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            showParserMessage(QString::fromLatin1("Could not parse config file '%1', line %2, column %3: %4.\n")
                              .arg(cf.fileName()).arg(parseError.line).arg(parseError.column).arg(parseError.errorString()),
                              ErrorMessage);
            exit(1);
        }

        if ((docs.size() != 2)
                || (docs.first().toMap().value(qSL("formatType")).toString() != qL1S("am-configuration"))
                || (docs.first().toMap().value(qSL("formatVersion")).toInt(0) != 1)
                || (docs.at(1).type() != QVariant::Map)) {
            showParserMessage(QString::fromLatin1("Could not parse config file '%1': Invalid document format.\n")
                              .arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        d->mergeConfig(docs.at(1).toMap());
    }
}

QString Configuration::mainQmlFile() const
{
    if (!d->clp.positionalArguments().isEmpty())
        return *--d->clp.positionalArguments().cend();
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

bool Configuration::forceSingleProcess() const
{
    return d->config<bool>("force-single-process", { qSL("flags"), qSL("forceSingleProcess") });
}

QString Configuration::singleApp() const
{
    return d->clp.value(qSL("single-app"));
}

QStringList Configuration::loggingRules() const
{
    return d->config<QStringList>("logging-rule", { qSL("logging"), qSL("rules") });
}

QVariantList Configuration::installationLocations() const
{
    return d->findInConfigFile({ qSL("installationLocations") }).toList();
}

QVariantMap Configuration::containerConfigurations() const
{
    return d->findInConfigFile({ qSL("containers") }).toMap();
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

QVariantMap Configuration::additionalUiConfiguration() const
{
    return d->findInConfigFile({ qSL("ui"), qSL("additionalConfiguration") }).toMap();
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
    return d->findInConfigFile({ qSL("installer"), qSL("caCertificates") }).toStringList();
}
