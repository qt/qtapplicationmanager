/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/
#include <QCommandLineParser>
#include <QFile>
#include <QVariantMap>
#include <QFileInfo>
#include <QDebug>

#include <functional>

#include "qtyaml.h"
#include "configuration.h"

#if !defined(AM_CONFIG_FILE)
#  define AM_CONFIG_FILE "/opt/am/config.yaml"
#endif


class ConfigurationPrivate
{
public:
    QVariant findInConfigFile(const QStringList &path, bool *found = 0);

    template <typename T> T config(const QString &clname, const QStringList &cfname)
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

template<> bool ConfigurationPrivate::config(const QString &clname, const QStringList &cfname)
{
    return clp.isSet(clname) || findInConfigFile(cfname).toBool();
}

template<> QString ConfigurationPrivate::config(const QString &clname, const QStringList &cfname)
{
    QString clval = clp.value(clname);
    bool cffound;
    QString cfval = findInConfigFile(cfname, &cffound).toString();
    return (clp.isSet(clname) || !cffound) ? clval : cfval;
}

template<> QStringList ConfigurationPrivate::config(const QString &clname, const QStringList &cfname)
{
    return clp.values(clname) + findInConfigFile(cfname).toStringList();
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
    d->clp.setApplicationDescription("Pelagicore ApplicationManager"
                                     "\n\n"
                                     "In addition to the commandline options below, the following environment\n"
                                     "variables can be set:\n\n"
                                     "  AM_FAKE_SUDO      if set to 1, no root privileges will be acquired\n"
                                     "  AM_STARTUP_TIMER  if set to 1, a startup performance analysis will be printed\n"
                                     "                    on the console. Anything other than 1 will interpreted\n"
                                     "                    as the name of a file that is used instead of the console\n");
    d->clp.addHelpOption();
    d->clp.addVersionOption();

    d->clp.addPositionalArgument("qml-file",   "the main QML file.");

    d->clp.addOption({ { "c", "config-file" }, "load configuration from file (can be given multiple times).", "files", AM_CONFIG_FILE });
    d->clp.addOption({ "database",             "application database.", "file", "/opt/am/apps.db" });
    d->clp.addOption({ { "r", "recreate-database" },  "recreate the application database." });
    d->clp.addOption({ "builtin-apps-manifest-dir",   "base directory for built-in application manifests.", "dir" });
    d->clp.addOption({ "installed-apps-manifest-dir", "base directory for installed application manifests.", "dir", "/opt/am/manifests" });
    d->clp.addOption({ "app-image-mount-dir", "base directory where application images are mounted to.", "dir", "/opt/am/image-mounts" });
#if defined(QT_DBUS_LIB)
    d->clp.addOption({ "dbus",                 "register on the specified D-Bus.", "<bus>|system|session|none", "session" });
#endif
    d->clp.addOption({ "fullscreen",           "display in full-screen." });
    d->clp.addOption({ "I",                    "additional QML import path.", "dir" });
    d->clp.addOption({ "verbose",              "verbose output." });
    d->clp.addOption({ "slow-animations",      "run all animations in slow motion." });
    d->clp.addOption({ "load-dummydata",       "loads QML dummy-data." });
    d->clp.addOption({ "no-security",          "disables all security related checks (dev only!)" });
    d->clp.addOption({ "no-ui-watchdog",       "disables detecting hung UI applications (e.g. via Wayland's ping/pong)." });
    d->clp.addOption({ "force-single-process", "forces single-process mode even on a wayland enabled build." });
    d->clp.addOption({ "single-app",           "runs a single application only (ignores the database)", "info.yaml file" });
    d->clp.addOption({ "logging-rule",         "adds a standard Qt logging rule.", "rule" });

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
        showParserMessage(d->clp.errorText() + '\n', ErrorMessage);
        exit(1);
    }

    if (d->clp.isSet("version"))
        d->clp.showVersion();

    if (d->clp.isSet("help"))
        d->clp.showHelp();

    if (d->clp.positionalArguments().size() > 1) {
        showParserMessage("Only one main qml file can be specified.\n", ErrorMessage);
        exit(1);
    }

    QStringList configFilePaths = d->clp.values("config-file");
#if defined(Q_OS_ANDROID)
    if (!d->clp.isSet("config-file")) {
        if (!(QString sdFilePath = findOnSDCard("application-manager.conf")).isEmpty())
            configFilePaths = QStringList(sdFilePath);
    }
#endif

    foreach (const QString &configFilePath, configFilePaths) {
        QFile cf(configFilePath);
        if (!cf.open(QIODevice::ReadOnly)) {
            showParserMessage(QStringLiteral("Failed to open config file '%1' for reading.\n").arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        if (cf.size() > 1024*1024) {
            showParserMessage(QStringLiteral("Config file '%1' is too big (> 1MB).\n").arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(cf.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            showParserMessage(QStringLiteral("Could not parse config file '%1', line %2, column %3: %4.\n")
                              .arg(cf.fileName()).arg(parseError.line).arg(parseError.column).arg(parseError.errorString()),
                              ErrorMessage);
            exit(1);
        }

        if ((docs.size() != 2)
                || (docs.first().toMap().value("formatType").toString() != "am-configuration")
                || (docs.first().toMap().value("formatVersion").toInt(0) != 1)
                || (docs.at(1).type() != QVariant::Map)) {
            showParserMessage(QStringLiteral("Could not parse config file '%1': Invalid document format.\n")
                              .arg(cf.fileName()), ErrorMessage);
            exit(1);
        }

        d->mergeConfig(docs.at(1).toMap());
    }
}

QString Configuration::mainQmlFile() const
{
    if (!d->clp.positionalArguments().isEmpty())
        return d->clp.positionalArguments().last();
    else
        return d->findInConfigFile({ "ui", "mainQml" }).toString();
}


QString Configuration::database() const
{
    return d->config<QString>("database", { "applications", "database" });
}

bool Configuration::recreateDatabase() const
{
    return d->clp.isSet("recreate-database");
}

QString Configuration::builtinAppsManifestDir() const
{
    return d->config<QString>("builtin-apps-manifest-dir", { "applications", "builtinAppsManifestDir" });
}

QString Configuration::installedAppsManifestDir() const
{
    return d->config<QString>("installed-apps-manifest-dir", { "applications", "installedAppsManifestDir" });
}

QString Configuration::appImageMountDir() const
{
    return d->config<QString>("app-image-mount-dir", { "applications", "appImageMountDir" });
}

bool Configuration::fullscreen() const
{
    return d->config<bool>("fullscreen", { "ui", "fullscreen" });
}

QStringList Configuration::importPaths() const
{
    QStringList importPaths = d->config<QStringList>("I", { "ui", "importPaths" });

    for (int i = 0; i < importPaths.size(); ++i)
        importPaths[i] = QFileInfo(importPaths.at(i)).absoluteFilePath();

    return importPaths;
}

bool Configuration::verbose() const
{
    return d->clp.isSet("verbose");
}

bool Configuration::slowAnimations() const
{
    return d->clp.isSet("slow-animations");
}

bool Configuration::loadDummyData() const
{
    return d->config<bool>("load-dummydata", { "ui", "loadDummyData" });
}

bool Configuration::noSecurity() const
{
    return d->config<bool>("no-security", { "flags", "noSecurity" });
}

bool Configuration::noUiWatchdog() const
{
    return d->config<bool>("no-ui-watchdog", { "flags", "noUiWatchdog" });
}

bool Configuration::forceSingleProcess() const
{
    return d->config<bool>("force-single-process", { "flags", "forceSingleProcess" });
}

QString Configuration::singleApp() const
{
    return d->clp.value("single-app");
}

QStringList Configuration::loggingRules() const
{
    return d->config<QStringList>("logging-rule", { "logging", "rules" });
}

QVariantList Configuration::installationLocations() const
{
    return d->findInConfigFile({ "installationLocations" }).toList();
}

QVariantMap Configuration::runtimeConfigurations() const
{
    return d->findInConfigFile({ "runtimes" }).toMap();
}

QVariantMap Configuration::dbusPolicy(const QString &interfaceName) const
{
    return d->findInConfigFile({ "dbus", interfaceName, "policy" }).toMap();
}

QString Configuration::dbusRegistration(const QString &interfaceName) const
{
    QString dbus = d->config<QString>("dbus", { "dbus", interfaceName, "register" });
    if (dbus == "none")
        dbus.clear();
    return dbus;
}

QVariantMap Configuration::additionalUiConfiguration() const
{
    return d->findInConfigFile({ "ui", "additionalConfiguration" }).toMap();
}

bool Configuration::applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const
{
    bool found = false;
    QVariantMap map = d->findInConfigFile({ "installer", "applicationUserIdSeparation" }, &found).toMap();

    if (found) {
        auto idFromMap = [&map](const char *key) -> uint {
            bool ok;
            uint value = map.value(key).toUInt(&ok);
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
    qreal idleLoad = d->findInConfigFile({ "quicklaunch", "idleLoad" }, &found).toReal(&conversionOk);
    return (found && conversionOk) ? idleLoad : qreal(0);
}

int Configuration::quickLaunchRuntimesPerContainer() const
{
    bool found, conversionOk;
    int rpc = d->findInConfigFile({ "quicklaunch", "runtimesPerContainer" }, &found).toInt(&conversionOk);
    return (found && conversionOk && rpc > 1 && rpc < 10) ? rpc : 1;
}
