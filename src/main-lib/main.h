// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef MAIN_H
#define MAIN_H

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
QT_FORWARD_DECLARE_CLASS(QDBusServer)

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
class DBusContextAdaptor;


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

    void setup(const Configuration *cfg) noexcept(false);
    QT_DEPRECATED_X("Replaced by loadQml() in 6.7 - will be removed in 6.9")
    void loadQml(bool loadDummyData) noexcept(false);
    void loadQml() noexcept(false);
    QT_DEPRECATED_X("Replaced by showWindow() in 6.8 - will be removed in 6.10")
    void showWindow(bool showFullscreen);
    void showWindow();

    Q_INVOKABLE void shutDown(int exitCode = 0);

    QQmlApplicationEngine *qmlEngine() const;

    static int exec();

protected:
    void registerResources(const QStringList &resources) const;
    void loadStartupPlugins(const QStringList &startupPluginPaths) noexcept(false);
    void parseSystemProperties(const QVariantMap &rawSystemProperties);
    void setupDBus(const Configuration *cfg);
    void setMainQmlFile(const QString &mainQml) noexcept(false);
    void setupSingleOrMultiProcess(const Configuration *cfg) noexcept(false);
    void setupRuntimesAndContainers(const Configuration *cfg);
    void loadPackageDatabase(const Configuration *cfg) noexcept(false);
    void setupIntents(const Configuration *cfg) noexcept(false);
    void setupSingletons(const Configuration *cfg) noexcept(false);
    void setupQuickLauncher(const Configuration *cfg);
    void setupInstaller(const Configuration *cfg) noexcept(false);
    void registerPackages();

    void setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle = QString());
    void setupWindowManager(const Configuration *cfg);
    void createInstanceInfoFile(const QString &instanceId) noexcept(false);

    enum SystemProperties {
        SP_ThirdParty = 0,
        SP_BuiltIn,
        SP_SystemUi
    };

    QString hardwareId() const;

private:
    static int &preConstructor(int &argc, char **argv, InitFlags initFlags);

    QString registerDBusObject(QDBusAbstractAdaptor *adaptor, const QString &dbusName,
                               const QString &serviceName, const QString &path) noexcept(false);

private:
    bool m_isSingleProcessMode = false;
    bool m_isRunningOnEmbedded = false;
    QUrl m_mainQml;
    QString m_mainQmlLocalFile;
    bool m_showFullscreen;
    bool m_loadDummyData = false; //TODO: remove in 6.9

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

    QVariantMap m_infoFileContents;

    QDBusServer *m_p2pServer = nullptr;
    QHash<QString, DBusContextAdaptor *> m_p2pAdaptors;
    bool m_p2pFailed = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Main::InitFlags)

QT_END_NAMESPACE_AM


#endif // MAIN_H
