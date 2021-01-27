/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QUrl>
#include <QtAppManCommon/global.h>
#include <functional>

#if defined(AM_HEADLESS)
#  include <QCoreApplication>
typedef QCoreApplication MainBase;
#elif defined(AM_ENABLE_WIDGETS)
#  include <QApplication>
#  include <QSurfaceFormat>
typedef QApplication MainBase;
#else
#  include <QGuiApplication>
#  include <QSurfaceFormat>
typedef QGuiApplication MainBase;
#endif

#include <QtAppManSharedMain/sharedmain.h>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QQmlApplicationEngine)
QT_FORWARD_DECLARE_CLASS(QQuickView)
QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

class StartupInterface;

QT_BEGIN_NAMESPACE_AM

class ApplicationInfo;
class StartupTimer;
class ApplicationIPCManager;
class PackageDatabase;
class PackageManager;
class ApplicationManager;
class ApplicationInstaller;
class NotificationManager;
class IntentServer;
class WindowManager;
class QuickLauncher;
class SystemMonitor;
class Configuration;

class Main : public MainBase, protected SharedMain
{
    Q_OBJECT
    Q_PROPERTY(bool singleProcessMode READ isSingleProcessMode)

public:
    Main(int &argc, char **argv);
    ~Main();

    bool isSingleProcessMode() const;
    bool isRunningOnEmbedded() const;

    Q_DECL_DEPRECATED void setup(const Configuration *cfg, const QStringList &deploymentWarnings)
                                                                      Q_DECL_NOEXCEPT_EXPR(false);
    void setup(const Configuration *cfg) Q_DECL_NOEXCEPT_EXPR(false);
    void loadQml(bool loadDummyData) Q_DECL_NOEXCEPT_EXPR(false);
    void showWindow(bool showFullscreen);

    Q_INVOKABLE void shutDown(int exitCode = 0);

    QQmlApplicationEngine *qmlEngine() const;

protected:
    void registerResources(const QStringList &resources) const;
    void loadStartupPlugins(const QStringList &startupPluginPaths) Q_DECL_NOEXCEPT_EXPR(false);
    void parseSystemProperties(const QVariantMap &rawSystemProperties);
    void setupDBus(const std::function<QString(const char *)> &busForInterface,
                   const std::function<QVariantMap(const char *)> &policyForInterface);
    void setMainQmlFile(const QString &mainQml) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingleOrMultiProcess(bool forceSingleProcess, bool forceMultiProcess) Q_DECL_NOEXCEPT_EXPR(false);
    void setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QVariantMap &openGLConfiguration,
                                    const QVariantMap &containerConfigurations, const QStringList &containerPluginPaths,
                                    const QStringList &iconThemeSearchPaths, const QString &iconThemeName);
    void loadPackageDatabase(bool recreateDatabase, const QString &singlePackage) Q_DECL_NOEXCEPT_EXPR(false);
    void setupIntents(int disambiguationTimeout, int startApplicationTimeout,
                      int replyFromApplicationTimeout, int replyFromSystemTimeout) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration,
                         int quickLaunchRuntimesPerContainer, qreal quickLaunchIdleLoad) Q_DECL_NOEXCEPT_EXPR(false);
    void setupInstaller(bool devMode, bool allowUnsigned, const QStringList &caCertificatePaths,
                        const std::function<bool(uint *, uint *, uint *)> &userIdSeparation) Q_DECL_NOEXCEPT_EXPR(false);
    void registerPackages();

    void setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle = QString());
    void setupWindowTitle(const QString &title, const QString &iconPath);
    void setupWindowManager(const QString &waylandSocketName, const QVariantList &waylandExtraSockets, bool slowAnimations, bool uiWatchdog);
    void setupTouchEmulation(bool enableTouchEmulation);

    void setupShellServer(const QString &telnetAddress, quint16 telnetPort) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSSDPService() Q_DECL_NOEXCEPT_EXPR(false);

    enum SystemProperties {
        SP_ThirdParty = 0,
        SP_BuiltIn,
        SP_SystemUi
    };

    QString hardwareId() const;

private:
#if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
    void registerDBusObject(QDBusAbstractAdaptor *adaptor, QString dbusName, const char *serviceName,
                            const char *interfaceName, const char *path) Q_DECL_NOEXCEPT_EXPR(false);
#endif
    static int &preConstructor(int &argc);

private:
    bool m_isSingleProcessMode = false;
    bool m_isRunningOnEmbedded = false;
    QUrl m_mainQml;
    QString m_mainQmlLocalFile;

    QQmlApplicationEngine *m_engine = nullptr;
    QQuickView *m_view = nullptr; // only set if we allocate the window ourselves

    PackageDatabase *m_packageDatabase = nullptr;
    PackageManager *m_packageManager = nullptr;
    ApplicationManager *m_applicationManager = nullptr;
    ApplicationIPCManager *m_applicationIPCManager = nullptr;
#if !defined(AM_DISABLE_INSTALLER)
    ApplicationInstaller *m_applicationInstaller = nullptr;
#endif
    NotificationManager *m_notificationManager = nullptr;
    IntentServer *m_intentServer = nullptr;
    WindowManager *m_windowManager = nullptr;
    QuickLauncher *m_quickLauncher = nullptr;
    QVector<StartupInterface *> m_startupPlugins;
    QVector<QVariantMap> m_systemProperties;

    bool m_noSecurity = false;
    QStringList m_builtinAppsManifestDirs;
    QString m_installationDir;
    QString m_documentDir;
};

QT_END_NAMESPACE_AM
