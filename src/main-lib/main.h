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

#pragma once

#include <QtAppManCommon/global.h>
#include <functional>

#if defined(AM_HEADLESS)
#  include <QCoreApplication>
typedef QCoreApplication MainBase;
#else
#  include <QGuiApplication>
typedef QGuiApplication MainBase;
#endif

#include <QtAppManInstaller/installationlocation.h>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QQmlApplicationEngine)
QT_FORWARD_DECLARE_STRUCT(QQmlDebuggingEnabler)
QT_FORWARD_DECLARE_CLASS(QQuickView)
QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

class StartupInterface;

QT_BEGIN_NAMESPACE_AM

class Application;
class StartupTimer;
class ApplicationIPCManager;
class ApplicationDatabase;
class ApplicationManager;
class ApplicationInstaller;
class NotificationManager;
class WindowManager;
class QuickLauncher;
class SystemMonitor;
class DefaultConfiguration;

class Main : public MainBase
{
    Q_OBJECT
    Q_PROPERTY(bool singleProcessMode READ isSingleProcessMode)

public:
    Main(int &argc, char **argv);
    ~Main();

    bool isSingleProcessMode() const;

    void setup(const DefaultConfiguration *cfg) Q_DECL_NOEXCEPT_EXPR(false);

    void shutDown();

    QQmlApplicationEngine *qmlEngine() const;

protected:
    void setupQmlDebugging(bool qmlDebugging);
    void setupLoggingRules(bool verbose, const QStringList &loggingRules);
    void loadStartupPlugins(const QStringList &startupPluginPaths) Q_DECL_NOEXCEPT_EXPR(false);
    void parseSystemProperties(const QVariantMap &rawSystemProperties);
    void setupDBus(bool startSessionBus) Q_DECL_NOEXCEPT_EXPR(false);
    void registerDBusInterfaces(const std::function<QString(const char *)> &busForInterface,
                                const std::function<QVariantMap(const char *)> &policyForInterface);
    void setMainQmlFile(const QString &mainQml) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingleOrMultiProcess(bool forceSingleProcess, bool forceMultiProcess) Q_DECL_NOEXCEPT_EXPR(false);
    void setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QVariantMap &containerConfigurations,
                                    const QStringList &containerPluginPaths);
    void setupManifestPaths(const QStringList &builtinAppManifestPaths, const QString &installedAppsManifestDir);
    void loadApplicationDatabase(const QString &databasePath, bool recreateDatabase,
                                 const QString &singleApp) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration,
                         qreal quickLaunchRuntimesPerContainer, int quickLaunchIdleLoad) Q_DECL_NOEXCEPT_EXPR(false);
    void setupInstaller(const QVariantList &installationLocations, const QString &appImageMountDir,
                        const QStringList &caCertificatePaths, const std::function<bool(uint *, uint *, uint *)> &userIdSeparation) Q_DECL_NOEXCEPT_EXPR(false);

    void setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle = QString());
    void setupWindowTitle(const QString &title, const QString &iconPath);
    void setupWindowManager(const QString &waylandSocketName, bool slowAnimations, bool uiWatchdog);

    void loadQml(bool loadDummyData) Q_DECL_NOEXCEPT_EXPR(false);
    void showWindow(bool showFullscreen);
    void setupShellServer(const QString &telnetAddress, quint16 telnetPort) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSSDPService() Q_DECL_NOEXCEPT_EXPR(false);

    enum SystemProperties {
        SP_ThirdParty = 0,
        SP_BuiltIn,
        SP_SystemUi
    };

    QString hardwareId() const;

private:
    void loadDummyDataFiles();

#if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
    const char *dbusInterfaceName(QObject *o) const Q_DECL_NOEXCEPT_EXPR(false);
    void registerDBusObject(QDBusAbstractAdaptor *adaptor, const QString &dbusName, const char *serviceName,
                            const char *interfaceName, const char *path) Q_DECL_NOEXCEPT_EXPR(false);
#endif
    static QVector<const Application *> scanForApplication(const QString &singleAppInfoYaml,
                                                           const QStringList &builtinAppsDirs) Q_DECL_NOEXCEPT_EXPR(false);
    static QVector<const Application *> scanForApplications(const QStringList &builtinAppsDirs,
                                                            const QString &installedAppsDir,
                                                            const QVector<InstallationLocation> &installationLocations) Q_DECL_NOEXCEPT_EXPR(false);

private:
    QVector<InstallationLocation> m_installationLocations;
    bool m_isSingleProcessMode = false;
    QString m_mainQml;

    QQmlDebuggingEnabler *m_debuggingEnabler = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickView *m_view = nullptr; // only set if we allocate the window ourselves

    QScopedPointer<ApplicationDatabase> m_applicationDatabase;
    ApplicationManager *m_applicationManager = nullptr;
    ApplicationIPCManager *m_applicationIPCManager = nullptr;
    ApplicationInstaller *m_applicationInstaller = nullptr;
    NotificationManager *m_notificationManager = nullptr;
    WindowManager *m_windowManager = nullptr;
    QuickLauncher *m_quickLauncher = nullptr;
    SystemMonitor *m_systemMonitor = nullptr;
    QVector<StartupInterface *> m_startupPlugins;
    QVector<QVariantMap> m_systemProperties;

    bool m_noSecurity = false;
    QStringList m_builtinAppsManifestDirs;
    QString m_installedAppsManifestDir;
};

QT_END_NAMESPACE_AM
