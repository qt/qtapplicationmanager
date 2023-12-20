// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QThread>

#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/logging.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManCommon/crashhandler.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManSharedMain/notification.h>

#if defined(QT_WAYLANDCLIENT_LIB)
#  include <QWindow>
#  include "waylandqtamclientextension_p.h"
#endif
#include "dbus-utilities.h"
#include "qml-utilities.h"
#include "applicationinterface_interface.h"
#include "runtimeinterface_interface.h"
#include "notifications_interface.h"
#include "intentclientdbusimplementation.h"
#include "intentclient.h"
#include "dbusnotificationimpl.h"
#include "waylandapplicationmanagerwindowimpl.h"
#include "applicationmain.h"

#ifdef interface // in case windows.h got included somehow
#  undef interface
#endif

using namespace Qt::StringLiterals;


AM_QML_REGISTER_TYPES(QtApplicationManager)
AM_QML_REGISTER_TYPES(QtApplicationManager_Application)

QT_BEGIN_NAMESPACE_AM

ApplicationMain *ApplicationMain::s_instance = nullptr;

ApplicationMain::ApplicationMain(int &argc, char **argv) noexcept
    : ApplicationMainBase(SharedMain::preConstructor(argc), argv)
    , SharedMain()
{
    if (ApplicationMain::s_instance)
        qCritical("ERROR: only one instance of ApplicationMain is allowed");
    s_instance = this;

    NotificationImpl::setFactory([this](Notification *notification, const QString &) {
        return new DBusNotificationImpl(notification, this);
    });
    WaylandApplicationManagerWindowImpl::setFactory([this](ApplicationManagerWindow *window) {
        return new WaylandApplicationManagerWindowImpl(window, this);
    });
    WaylandApplicationManagerWindowAttachedImpl::setFactory([](ApplicationManagerWindowAttached *windowAttached,
                                                               QQuickItem *attacheeItem) {
        return new WaylandApplicationManagerWindowAttachedImpl(windowAttached, attacheeItem);
    });
}

ApplicationMain::~ApplicationMain()
{ }

ApplicationMain *ApplicationMain::instance()
{
    return s_instance;
}

void ApplicationMain::registerWaylandExtensions() Q_DECL_NOEXCEPT
{
#if defined(QT_WAYLANDCLIENT_LIB)
     m_waylandExtension.reset(new WaylandQtAMClientExtension());
     connect(m_waylandExtension.get(), &WaylandQtAMClientExtension::windowPropertyChanged,
             this, &ApplicationMain::windowPropertyChanged);
#endif
}

QString ApplicationMain::baseDir() const
{
    return m_baseDir;
}

QVariantMap ApplicationMain::runtimeConfiguration() const
{
    return m_runtimeConfiguration;
}

QByteArray ApplicationMain::securityToken() const
{
    return m_securityToken;
}

bool ApplicationMain::slowAnimations() const
{
    return m_slowAnimations;
}

void ApplicationMain::setSlowAnimations(bool slow)
{
    if (slow != m_slowAnimations) {
        m_slowAnimations = slow;
        emit slowAnimationsChanged(slow);
    }
}

QStringList ApplicationMain::loggingRules() const
{
    return m_loggingRules;
}

QVariant ApplicationMain::useAMConsoleLogger() const
{
    return m_useAMConsoleLogger;
}

QString ApplicationMain::dltLongMessageBehavior() const
{
    return m_dltLongMessageBehavior;
}

QString ApplicationMain::p2pDBusName() const
{
    return u"am"_s;
}

QString ApplicationMain::notificationDBusName() const
{
    return m_dbusAddressNotifications.isEmpty() ? QString() : u"am_notification_bus"_s;
}

QVariantMap ApplicationMain::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

QString ApplicationMain::iconThemeName() const
{
    return m_iconThemeName;
}

QStringList ApplicationMain::iconThemeSearchPaths() const
{
    return m_iconThemeSearchPaths;
}

QVariantMap ApplicationMain::windowProperties(QWindow *window) const
{
#if defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        return m_waylandExtension->windowProperties(window);
#else
    Q_UNUSED(window)
#endif
    return QVariantMap();
}

bool ApplicationMain::setWindowProperty(QWindow *window, const QString &name, const QVariant &value)
{
#if defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        return m_waylandExtension->setWindowProperty(window, name, value);
#else
    Q_UNUSED(window)
    Q_UNUSED(name)
    Q_UNUSED(value)
#endif
    return false;
}

void ApplicationMain::clearWindowPropertyCache(QWindow *window)
{
#if defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        m_waylandExtension->clearWindowPropertyCache(window);
#else
    Q_UNUSED(window)
#endif
}

QString ApplicationMain::applicationId() const
{
    return m_application.value(u"id"_s).toString();
}

QVariantMap ApplicationMain::applicationName() const
{
    return m_application.value(u"displayName"_s).toMap();
}

QUrl ApplicationMain::applicationIcon() const
{
    return m_application.value(u"displayIcon"_s).toUrl();
}

QString ApplicationMain::applicationVersion() const
{
    return m_application.value(u"version"_s).toString();
}

QVariantMap ApplicationMain::applicationProperties() const
{
    return m_application.value(u"applicationProperties"_s).toMap();
}

QVariantMap ApplicationMain::systemProperties() const
{
    return m_systemProperties;
}

void ApplicationMain::setApplication(const QVariantMap &application)
{
    m_application = application;
}

void ApplicationMain::setSystemProperties(const QVariantMap &properties)
{
    m_systemProperties = properties;
}


void ApplicationMain::setup()
{
    registerWaylandExtensions();
    loadConfiguration();
    setupLogging(false, loggingRules(), QString(), useAMConsoleLogger());
    setupDBusConnections();
    connectDBusInterfaces();
}

void ApplicationMain::loadConfiguration(const QByteArray &configYaml) Q_DECL_NOEXCEPT_EXPR(false)
{
    try {
        QVector<QVariant> docs = YamlParser::parseAllDocuments(configYaml.isEmpty() ? qgetenv("AM_CONFIG")
                                                                                    : configYaml);
        if (docs.size() == 1)
            m_configuration = docs.first().toMap();
    } catch (const Exception &e) {
        throw Exception("Runtime launcher could not parse the YAML configuration coming from the "
                        "application manager: %1").arg(e.errorString());
    }

    m_baseDir = m_configuration.value(u"baseDir"_s).toString() + u'/';
    m_runtimeConfiguration = m_configuration.value(u"runtimeConfiguration"_s).toMap();
    m_securityToken = QByteArray::fromHex(m_configuration.value(u""_s).toString().toLatin1());
    m_systemProperties = m_configuration.value(u"systemProperties"_s).toMap();

    QVariantMap loggingConfig = m_configuration.value(u"logging"_s).toMap();
    m_loggingRules = variantToStringList(loggingConfig.value(u"rules"_s));
    m_useAMConsoleLogger = loggingConfig.value(u"useAMConsoleLogger"_s);
    m_dltLongMessageBehavior = loggingConfig.value(u"dltLongMessageBehavior"_s).toString();

    QVariantMap dbusConfig = m_configuration.value(u"dbus"_s).toMap();
    m_dbusAddressP2P = dbusConfig.value(u"p2p"_s).toString();
    m_dbusAddressNotifications = dbusConfig.value(u"org.freedesktop.Notifications"_s).toString();

    QVariantMap uiConfig = m_configuration.value(u"ui"_s).toMap();
    m_slowAnimations = uiConfig.value(u"slowAnimations"_s).toBool();
    m_openGLConfiguration = uiConfig.value(u"opengl"_s).toMap();
    m_iconThemeName = uiConfig.value(u"iconThemeName"_s).toString();
    m_iconThemeSearchPaths = uiConfig.value(u"iconThemeSearchPaths"_s).toStringList();

    m_application = m_configuration.value(u"application"_s).toMap();

    // un-comment this if things go south:
    //qWarning() << "### LOG " << m_loggingRules;
    //qWarning() << "### DBUS" << dbusConfig;
    //qWarning() << "### UI  " << uiConfig;
    //qWarning() << "### RT  " << m_runtimeConfiguration;
    //qWarning() << "### SYSP" << m_systemProperties;
    //qWarning() << "### GL  " << m_openGLConfiguration;
    //qWarning() << "### APP " << m_application;

    // sanity checks
    if (m_baseDir == u"/")
        throw Exception("Runtime launcher received an empty baseDir");
    if (loggingConfig.isEmpty())
        throw Exception("Runtime launcher received no logging configuration");
    if (dbusConfig.isEmpty())
        throw Exception("Runtime launcher received no D-Bus configuration");
}

void ApplicationMain::setupDBusConnections() Q_DECL_NOEXCEPT_EXPR(false)
{
    if (m_dbusAddressP2P.isEmpty())
        throw Exception("No address available to connect to the P2P D-Bus");

    auto dbusConnection = QDBusConnection::connectToPeer(m_dbusAddressP2P, p2pDBusName());

    if (!dbusConnection.isConnected())
        throw Exception("could not connect to the P2P D-Bus via: %1").arg(m_dbusAddressP2P);

    qCDebug(LogRuntime) << "Connected to the P2P D-Bus via:" << m_dbusAddressP2P;

    if (!m_dbusAddressNotifications.isEmpty()) {
        if (m_dbusAddressNotifications == u"system")
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SystemBus, notificationDBusName());
        else if (m_dbusAddressNotifications == u"session")
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, notificationDBusName());
        else
            dbusConnection = QDBusConnection::connectToBus(m_dbusAddressNotifications, notificationDBusName());

        if (!dbusConnection.isConnected())
            throw Exception("could not connect to the Notification D-Bus via: %1").arg(m_dbusAddressNotifications);

        qCDebug(LogRuntime) << "Connected to the Notification D-Bus via:" << m_dbusAddressNotifications;
    } else {
        qCWarning(LogDeployment) << "Notifications are not supported by this configuration";
    }
}

template<typename T>
static T *tryConnectToDBusInterface(const QString &service, const QString &path,
                                    const QString &connectionName, QObject *parent)
{
    // we are working with very small delays in the milli-second range here, so a linear factor
    // to support valgrind would have to be very large and probably conflict with usage elsewhere
    // in the codebase, where the ranges are normally in the seconds.
    static const int timeout = timeoutFactor() * timeoutFactor();

    QDBusConnection conn(connectionName);

    if (!conn.isConnected())
        return nullptr;
    if (!service.isEmpty() && conn.interface()) {
        // the 'T' constructor can block up to 25sec (!), if the service is not registered!
        if (!conn.interface()->isServiceRegistered(service))
            return nullptr;
    }

    QElapsedTimer timer;
    timer.start();

    do {
        T *iface = new T(service, path, conn, parent);
        if (!iface->lastError().isValid())
            return iface;
        delete iface;
        QThread::msleep(static_cast<unsigned long>(timeout));
    } while (timer.elapsed() < (100 * timeout)); // 100msec base line

    return nullptr;
}

void ApplicationMain::connectDBusInterfaces(bool isRuntimeLauncher) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_dbusApplicationInterface = tryConnectToDBusInterface<IoQtApplicationManagerApplicationInterfaceInterface>(
        { }, u"/ApplicationInterface"_s, p2pDBusName(), this);

    if (!m_dbusApplicationInterface)
        throw Exception("could not connect to the ApplicationInterface on the P2P D-Bus");
    if (!m_dbusApplicationInterface->isValid()) {
        throw Exception("ApplicationInterface on the P2P D-Bus is not valid: %1")
            .arg(m_dbusApplicationInterface->lastError().message());
    }

    bool ok = true;

    ok = ok && connect(m_dbusApplicationInterface, &IoQtApplicationManagerApplicationInterfaceInterface::quit,
                       this, &ApplicationMain::quit);
    ok = ok && connect(m_dbusApplicationInterface, &IoQtApplicationManagerApplicationInterfaceInterface::memoryLowWarning,
                       this, &ApplicationMain::memoryLowWarning);
    ok = ok && connect(m_dbusApplicationInterface, &IoQtApplicationManagerApplicationInterfaceInterface::memoryCriticalWarning,
                       this, &ApplicationMain::memoryCriticalWarning);
    ok = ok && connect(m_dbusApplicationInterface, &IoQtApplicationManagerApplicationInterfaceInterface::openDocument,
                       this, &ApplicationMain::openDocument);
    ok = ok && connect(m_dbusApplicationInterface, &IoQtApplicationManagerApplicationInterfaceInterface::slowAnimationsChanged,
                       this, &ApplicationMain::slowAnimationsChanged);

    if (!ok) {
        throw Exception("could not connect the ApplicationInterface via D-Bus: %1")
            .arg(m_dbusApplicationInterface->lastError().name());
    }

    if (isRuntimeLauncher) {
        m_dbusRuntimeInterface = tryConnectToDBusInterface<IoQtApplicationManagerRuntimeInterfaceInterface>(
            { }, u"/RuntimeInterface"_s, p2pDBusName(), this);

        if (!m_dbusRuntimeInterface)
            throw Exception("could not connect to the RuntimeInterface on the P2P D-Bus");
        if (!m_dbusRuntimeInterface->isValid()) {
            throw Exception("RuntimeInterface on the P2P D-Bus is not valid: %1")
                      .arg(m_dbusRuntimeInterface->lastError().message());
        }

        if (!connect(m_dbusRuntimeInterface, &IoQtApplicationManagerRuntimeInterfaceInterface::startApplication,
                     this, &ApplicationMain::startApplication)) {
            throw Exception("could not connect the RuntimeInterface signals via D-Bus: %1")
                      .arg(m_dbusRuntimeInterface->lastError().name());
        }
    }


    if (!m_dbusAddressNotifications.isEmpty()) {
        m_dbusNotificationInterface = tryConnectToDBusInterface<OrgFreedesktopNotificationsInterface>(
            u"org.freedesktop.Notifications"_s, u"/org/freedesktop/Notifications"_s,
            notificationDBusName(), this);

        if (m_dbusNotificationInterface) {
            ok = ok && connect(m_dbusNotificationInterface, &OrgFreedesktopNotificationsInterface::NotificationClosed,
                               this, [this](uint notificationId, uint reason) {
                     Q_UNUSED(reason);

                     qDebug("Notification was closed signal: %u", notificationId);
                     for (const QPointer<Notification> &n : std::as_const(m_allNotifications)) {
                         if (n->notificationId() == notificationId) {
                             n->close();
                             m_allNotifications.removeAll(n);
                             break;
                         }
                     }

                 });
            ok = ok && connect(m_dbusNotificationInterface, &OrgFreedesktopNotificationsInterface::ActionInvoked,
                               this, [this](uint notificationId, const QString &actionId) {
                     qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
                     for (const QPointer<Notification> &n : std::as_const(m_allNotifications)) {
                         if (n->notificationId() == notificationId) {
                             n->triggerAction(actionId);
                             break;
                         }
                     }
                 });

            if (!ok) {
                qCritical("ERROR: could not connect the org.freedesktop.Notifications signals via D-Bus: %s",
                          qPrintable(m_dbusNotificationInterface->lastError().name()));

                delete m_dbusNotificationInterface;
                m_dbusNotificationInterface = nullptr;
            }

        } else {
            qCritical("ERROR: could not connect to the org.freedesktop.Notifications interface via D-Bus");
        }
    }

    auto intentClientDBusInterface = new IntentClientDBusImplementation(p2pDBusName(), this);
    if (!IntentClient::createInstance(intentClientDBusInterface))
        throw Exception("could not connect to the IntentInterface on the P2P D-Bus");

    m_dbusApplicationInterface->asyncCall(u"finishedInitialization"_s);
}

uint QtAM::ApplicationMain::showNotification(Notification *notification)
{
    if (notification && m_dbusNotificationInterface) {
        uint newId = m_dbusNotificationInterface->Notify(applicationId(),
                                                         notification->notificationId(),
                                                         notification->icon().toString(),
                                                         notification->summary(),
                                                         notification->body(),
                                                         notification->libnotifyActionList(),
                                                         notification->libnotifyHints(),
                                                         notification->timeout());
        if (newId) {
            m_allNotifications << notification;
            return newId;
        }
    }
    return 0;
}

void ApplicationMain::closeNotification(Notification *notification)
{
    if (notification && m_dbusNotificationInterface)
        m_dbusNotificationInterface->CloseNotification(notification->notificationId());

}

Notification *ApplicationMain::createNotification(QObject *parent)
{
    return new Notification(parent, applicationId());
}

QT_END_NAMESPACE_AM

#include "moc_applicationmain.cpp"
