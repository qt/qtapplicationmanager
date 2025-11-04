// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONMAIN_H
#define APPLICATIONMAIN_H

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/openglconfiguration.h>
#include <QtAppManCommon/watchdogconfiguration.h>
#include <QtAppManSharedMain/sharedmain.h>
#if QT_CONFIG(am_widgets_support)
#  include <QtWidgets/QApplication>
using ApplicationMainBase = QApplication;
#else
#  include <QtGui/QGuiApplication>
using ApplicationMainBase = QGuiApplication;
#endif

#include <memory>


QT_FORWARD_DECLARE_CLASS(QWindow)
class IoQtApplicationManagerApplicationInterfaceInterface;
class IoQtApplicationManagerRuntimeInterfaceInterface;
class OrgFreedesktopNotificationsInterface;

QT_BEGIN_NAMESPACE_AM

class WaylandQtAMClientExtension;
class Notification;
class ApplicationInterfaceImpl;


class ApplicationMain : public ApplicationMainBase, public SharedMain
{
    Q_OBJECT
public:
    ApplicationMain(int &argc, char **argv) noexcept;
    virtual ~ApplicationMain() override;

    static ApplicationMain *instance();

public:
    void setup() noexcept(false);
    void loadConfiguration(const QByteArray &configYaml = QByteArray()) noexcept(false);
    void setupDBusConnections() noexcept(false);
    void connectDBusInterfaces(bool isRuntimeLauncher = false) noexcept(false);
    void registerWaylandExtensions() noexcept;

    // D-Bus names (use with QDBusConnection)
    QString p2pDBusName() const;
    QString notificationDBusName() const;

    // basic configuration
    QString baseDir() const;
    QVariantMap runtimeConfiguration() const;
    QByteArray securityToken() const;
    QStringList loggingRules() const;
    QVariant useAMConsoleLogger() const;
    QString dltLongMessageBehavior() const;
    OpenGLConfiguration openGLConfiguration() const;
    WatchdogConfiguration watchdogConfiguration() const;
    QString iconThemeName() const;
    QStringList iconThemeSearchPaths() const;

    // ApplicationInterface properties
    QString applicationId() const;
    QVariantMap applicationName() const;
    QUrl applicationIcon() const;
    QString applicationVersion() const;
    QVariantMap applicationProperties() const;
    QVariantMap systemProperties() const;
    bool slowAnimations() const;

    // These are needed for (Quick)Launchers that receive new data after the initial setup
    void setApplication(const QVariantMap &application);
    void setSystemProperties(const QVariantMap &properties);
    void setSlowAnimations(bool slow);

    // DBus ApplicationInterface
    Q_SIGNAL void quit();
    Q_SIGNAL void memoryLowWarning();
    Q_SIGNAL void memoryCriticalWarning();
    Q_SIGNAL void openDocument(const QString &documentUrl, const QString &mimeType);
    Q_SIGNAL void slowAnimationsChanged(bool isSlow);

    // Wayland Extension
    QVariantMap windowProperties(QWindow *window) const;
    bool setWindowProperty(QWindow *window, const QString &name, const QVariant &value);
    void clearWindowPropertyCache(QWindow *window);
    Q_SIGNAL void windowPropertyChanged(QWindow *window, const QString &name, const QVariant &value);

    // DBus RuntimeInterface
    Q_SIGNAL void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                                   const QString &mimeType, const QVariantMap &runtimeParams,
                                   const QVariantMap systemProperties);

    // DBus NotificationInterface
    Q_SIGNAL void notificationClosed(uint id, uint reason);
    Q_SIGNAL void notificationActionInvoked(uint id, const QString &actionKey);

    // DBusNotificationImplemetation API
    uint showNotification(Notification *notification);
    void closeNotification(Notification *notification);

    // Notification helper
    Notification *createNotification(QObject *parent = nullptr);

public:
    bool notify(QObject *receiver, QEvent *event) override;

private:
    static ApplicationMain *s_instance;
    Q_DISABLE_COPY_MOVE(ApplicationMain)

    QString m_baseDir;
    QVariantMap m_configuration;
    QVariantMap m_runtimeConfiguration;
    QByteArray m_securityToken;

    bool m_slowAnimations = false;
    OpenGLConfiguration m_openGLConfiguration;
    WatchdogConfiguration m_watchdogConfiguration;
    QString m_iconThemeName;
    QStringList m_iconThemeSearchPaths;

    QStringList m_loggingRules;
    QVariant m_useAMConsoleLogger;
    QString m_dltLongMessageBehavior;
#if defined(QT_WAYLANDCLIENT_LIB)
    std::unique_ptr<WaylandQtAMClientExtension> m_waylandExtension;
#endif
    QVariantMap m_application;
    QVariantMap m_systemProperties;

    std::unique_ptr<ApplicationInterfaceImpl> m_applicationInterfaceImpl;

    QString m_dbusAddressP2P;
    QString m_dbusAddressNotifications;

    IoQtApplicationManagerApplicationInterfaceInterface *m_dbusApplicationInterface = nullptr;
    IoQtApplicationManagerRuntimeInterfaceInterface *m_dbusRuntimeInterface = nullptr;
    OrgFreedesktopNotificationsInterface *m_dbusNotificationInterface = nullptr;

    QVector<QPointer<Notification>> m_allNotifications;
};


QT_END_NAMESPACE_AM

#endif // APPLICATIONMAIN_H
