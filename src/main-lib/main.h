// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>

#include <QtCore/QUrl>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/logging.h>
#include <QtAppManManager/sudo.h>

#if QT_CONFIG(am_widgets_support)
#  include <QtWidgets/QApplication>
#  include <QtGui/QSurfaceFormat>
QT_BEGIN_NAMESPACE_AM
using MainBase = QApplication;
QT_END_NAMESPACE_AM
#else
#  include <QtGui/QGuiApplication>
#  include <QtGui/QSurfaceFormat>
using MainBase = QGuiApplication;
#endif

#include <QtAppManSharedMain/sharedmain.h>
#include <QtCore/QVector>

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
class NotificationManager;
class IntentServer;
class WindowManager;
class QuickLauncher;
class SystemMonitor;
class Configuration;

class Main : public MainBase, protected SharedMain
{
    Q_OBJECT
    Q_PROPERTY(bool singleProcessMode READ isSingleProcessMode CONSTANT FINAL)

public:
    enum class InitFlag {
        InitializeLogging  = 0x01,
        ForkSudoServer     = 0x02
    };
    Q_DECLARE_FLAGS(InitFlags, InitFlag)
    Q_FLAG(InitFlags)

    Main(int &argc, char **argv, InitFlags initFlags = { });
    ~Main() override;

    bool isSingleProcessMode() const;
    bool isRunningOnEmbedded() const;

    void setup(const Configuration *cfg) Q_DECL_NOEXCEPT_EXPR(false);
    void loadQml(bool loadDummyData) Q_DECL_NOEXCEPT_EXPR(false);
    void showWindow(bool showFullscreen);

    Q_INVOKABLE void shutDown(int exitCode = 0);

    QQmlApplicationEngine *qmlEngine() const;

    static int exec();

protected:
    void registerResources(const QStringList &resources) const;
    void loadStartupPlugins(const QStringList &startupPluginPaths) Q_DECL_NOEXCEPT_EXPR(false);
    void parseSystemProperties(const QVariantMap &rawSystemProperties);
    void setupDBus(const std::function<QString(const char *)> &busForInterface,
                   const std::function<QVariantMap(const char *)> &policyForInterface, const QString &instanceId);
    void setMainQmlFile(const QString &mainQml) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingleOrMultiProcess(bool forceSingleProcess, bool forceMultiProcess) Q_DECL_NOEXCEPT_EXPR(false);
    void setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QStringList &runtimeAdditionalLaunchers,
                                    const QVariantMap &containerConfigurations, const QStringList &containerPluginPaths,
                                    const QVariantMap &openGLConfiguration,
                                    const QStringList &iconThemeSearchPaths, const QString &iconThemeName);
    void loadPackageDatabase(bool recreateDatabase, const QString &singlePackage) Q_DECL_NOEXCEPT_EXPR(false);
    void setupIntents(int disambiguationTimeout, int startApplicationTimeout,
                      int replyFromApplicationTimeout, int replyFromSystemTimeout) Q_DECL_NOEXCEPT_EXPR(false);
    void setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration) Q_DECL_NOEXCEPT_EXPR(false);
    void setupQuickLauncher(int quickLaunchRuntimesPerContainer, qreal quickLaunchIdleLoad,
                            int failedStartLimit, int failedStartLimitIntervalSec) Q_DECL_NOEXCEPT_EXPR(false);
    void setupInstaller(bool allowUnsigned, const QStringList &caCertificatePaths) Q_DECL_NOEXCEPT_EXPR(false);
    void registerPackages();

    void setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle = QString());
    void setupWindowTitle(const QString &title, const QString &iconPath);
    void setupWindowManager(const QString &waylandSocketName, const QVariantList &waylandExtraSockets,
                            bool slowAnimations, bool noUiWatchdog, bool allowUnknownUiClients);

    enum SystemProperties {
        SP_ThirdParty = 0,
        SP_BuiltIn,
        SP_SystemUi
    };

    QString hardwareId() const;

private:
    static int &preConstructor(int &argc, char **argv, InitFlags initFlags);

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
    NotificationManager *m_notificationManager = nullptr;
    IntentServer *m_intentServer = nullptr;
    WindowManager *m_windowManager = nullptr;
    QuickLauncher *m_quickLauncher = nullptr;
    QVector<StartupInterface *> m_startupPlugins;
    QVector<QVariantMap> m_systemProperties;

    bool m_noSecurity = false;
    bool m_developmentMode = false;
    QStringList m_builtinAppsManifestDirs;
    QString m_installationDir;
    QString m_documentDir;
    QString m_installationDirMountPoint;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Main::InitFlags)

QT_END_NAMESPACE_AM

