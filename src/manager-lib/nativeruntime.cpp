// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <memory>

#include <QCoreApplication>
#include <QProcess>
#include <QDBusServer>
#include <QDBusConnection>
#include <QDBusError>
#include <QTimer>
#include <QUuid>
#include <QLibraryInfo>
#include <QStringBuilder>
#include <QDBusConnection>
#include "dbuscontextadaptor.h"

#include "global.h"
#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "nativeruntime.h"
#include "qtyaml.h"
#include "applicationinterface.h"
#include "utilities.h"
#include "notificationmanager.h"
#include "dbus-utilities.h"
#include "processtitle.h"

#include "runtimeinterface_adaptor.h"
#include "applicationinterface_adaptor.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// You can enable this define to get all P2P-bus objects onto the session bus
// within io.qt.ApplicationManager, /Application<pid>/...

// #define EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS 1

#if QT_CONFIG(am_multi_process) && defined(Q_OS_LINUX)
QT_END_NAMESPACE_AM
#  include <dlfcn.h>
#  include <sys/socket.h>
#  include <signal.h>
QT_BEGIN_NAMESPACE_AM

static qint64 getDBusPeerPid(const QDBusConnection &conn)
{
    typedef bool (*am_dbus_connection_get_socket_t)(void *, int *);
    static am_dbus_connection_get_socket_t am_dbus_connection_get_socket = nullptr;

    if (!am_dbus_connection_get_socket)
        am_dbus_connection_get_socket = reinterpret_cast<am_dbus_connection_get_socket_t>(dlsym(RTLD_DEFAULT, "dbus_connection_get_socket"));

    if (!am_dbus_connection_get_socket)
        qFatal("ERROR: could not resolve 'dbus_connection_get_socket' from libdbus-1");

    int socketFd = -1;
    if (am_dbus_connection_get_socket(conn.internalPointer(), &socketFd)) {
        struct ucred ucred;
        socklen_t ucredSize = sizeof(struct ucred);
        if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucredSize) == 0)
            return ucred.pid;
    }
    return 0;
}

#endif

NativeRuntime::NativeRuntime(AbstractContainer *container, Application *app, NativeRuntimeManager *manager)
    : AbstractRuntime(container, app, manager)
    , m_isQuickLauncher(app == nullptr)
    , m_startedViaLauncher(manager->identifier() != u"native")
    , m_dbusApplicationInterface(DBusContextAdaptor::create<ApplicationInterfaceAdaptor>(this))
    , m_dbusRuntimeInterface(DBusContextAdaptor::create<RuntimeInterfaceAdaptor>(this))
{
    QDir().mkdir(u"/tmp/dbus-qtam"_s);
    QString dbusAddress = QUuid::createUuid().toString().mid(1,36);
    m_applicationInterfaceServer = new QDBusServer(u"unix:path=/tmp/dbus-qtam/dbus-qtam-"_s + dbusAddress, this);
    m_applicationInterfaceServer->setAnonymousAuthenticationAllowed(true);

    connect(m_applicationInterfaceServer, &QDBusServer::newConnection,
            this, [this](const QDBusConnection &connection) {
#if QT_CONFIG(am_multi_process) && defined(Q_OS_LINUX)
        qint64 pid = getDBusPeerPid(connection);
        if (pid <= 0) {
            QDBusConnection::disconnectFromPeer(connection.name());
            qCWarning(LogSystem) << "Could not retrieve peer pid on D-Bus connection attempt.";
            return;
        }

        // try direct PID mapping first, then check for sub-processes ... this happens when
        // for example running the app via gdbserver
        qint64 appmanPid = QCoreApplication::applicationPid();

        int level = 0;
        while ((pid > 1) && (pid != appmanPid) && (level < 5)) {
            if (applicationProcessId() == pid) {
                onDBusPeerConnection(connection);
                return;
            }
            pid = getParentPid(pid);
            ++level;
        }

        QDBusConnection::disconnectFromPeer(connection.name());
        qCWarning(LogSystem) << "Connection attempt on peer D-Bus from unknown pid:" << pid;
#else
        // getting the pid is not supported on e.g. macOS. Accepting everything is not secure
        // but it at least works
        onDBusPeerConnection(connection);
#endif
    });
}

QDBusServer *NativeRuntime::applicationInterfaceServer() const
{
    return m_applicationInterfaceServer;
}

NativeRuntime::~NativeRuntime()
{
    delete m_process;
}

bool NativeRuntime::isQuickLauncher() const
{
    return m_isQuickLauncher;
}

bool NativeRuntime::attachApplicationToQuickLauncher(Application *app)
{
    if (!app || !isQuickLauncher() || !m_startedViaLauncher)
        return false;

    m_isQuickLauncher = false;
    m_app = app;
    m_app->setCurrentRuntime(this);

    setState(Am::StartingUp);

    bool ret;
    if (!m_dbusConnection) {
        // we have no D-Bus connection yet, so hope for the best
        ret = true;
    } else {
        QDBusConnection connection(m_dbusConnectionName);
        emit applicationReadyOnPeerDBus(connection, app);
        ret = startApplicationViaLauncher();
    }

    setState(ret ? Am::Running : Am::NotRunning);
    return ret;
}

bool NativeRuntime::initialize()
{
    if (m_startedViaLauncher) {
        static QVector<QString> possibleLocations;
        if (possibleLocations.isEmpty()) {
            // try the main binaries directory
            possibleLocations.append(QCoreApplication::applicationDirPath());
            // try Qt's bin folder
            possibleLocations.append(QLibraryInfo::path(QLibraryInfo::BinariesPath));
            // try the AM's build directory
            possibleLocations.append(qApp->property("_am_build_dir").toString() + u"/bin"_s); // set by main.cpp
            // if everything fails, try to locate it in $PATH
            const auto paths = qgetenv("PATH").split(QDir::listSeparator().toLatin1());
            for (const auto &path : paths)
                possibleLocations.append(QString::fromLocal8Bit(path));
        }

        const QString launcherName = u"/appman-launcher-"_s + manager()->identifier();
        for (const QString &possibleLocation : std::as_const(possibleLocations)) {
            QFileInfo fi(possibleLocation + launcherName);

            if (fi.exists() && fi.isExecutable()) {
                m_container->setProgram(fi.absoluteFilePath());
                m_container->setBaseDirectory(fi.absolutePath());
                qCDebug(LogSystem) << "Using runtime launcher"<< fi.absoluteFilePath();
                return true;
            }
        }
        qCWarning(LogSystem) << "Could not find an" << launcherName.mid(1) << "executable in any of:\n"
                             << possibleLocations;
        return false;
    } else {
        if (!m_app)
            return false;

        m_container->setProgram(m_app->info()->absoluteCodeFilePath());
        m_container->setBaseDirectory(m_app->codeDir());
        return true;
    }
}

void NativeRuntime::shutdown(int exitCode, Am::ExitStatus status)
{
    // see NativeRuntime::stop() below
    if ((status == Am::CrashExit) && (exitCode == SIGTERM || exitCode == SIGKILL))
        status = Am::ForcedExit;

    if (!m_isQuickLauncher || m_connectedToApplicationInterface) {
        QByteArray cause = "exited";
        bool printWarning = false;
        switch (status) {
        case Am::ForcedExit:
            cause = "was force exited (" + QByteArray(exitCode == SIGTERM ? "terminated" : "killed") + ")";
            printWarning = true;
            break;
        case Am::CrashExit:
            cause = "received signal: " + QByteArray::number(exitCode) + " ("
                    + QByteArray(::strsignal(exitCode)) + ")";
            printWarning = true;
            break;
        default:
            if (exitCode != 0) {
                cause = "exited with code: " + QByteArray::number(exitCode);
                printWarning = true;
            }
            break;
        }

        if (printWarning) {
            qCWarning(LogSystem, "Runtime for application '%s' (pid: %lld) %s",
                      (m_app ? qPrintable(m_app->id()) : "<quicklauncher>"), m_process->processId(),
                      cause.constData());
        } else {
            qCDebug(LogSystem, "Runtime for application '%s' (pid: %lld) %s",
                    (m_app ? qPrintable(m_app->id()) : "<quicklauncher>"), m_process->processId(),
                    cause.constData());
        }
    }
    m_connectedToApplicationInterface = m_dbusConnection = false;

    QDBusConnection connection(m_dbusConnectionName);
    emit applicationDisconnectedFromPeerDBus(connection, application());

    emit finished(exitCode, status);

    if (m_app)
        m_app->setCurrentRuntime(nullptr);
    setState(Am::NotRunning);

    deleteLater();
}

bool NativeRuntime::start()
{
    switch (state()) {
    case Am::StartingUp:
    case Am::Running:
        return true;
    case Am::ShuttingDown:
        return false;
    case Am::NotRunning:
        break;
    }

    QVariantMap dbusConfig = {
        { u"p2p"_s, applicationInterfaceServer()->address() },
        { u"org.freedesktop.Notifications"_s, NotificationManager::instance()->property("_am_dbus_name").toString()}
    };

    QVariantMap loggingConfig = {
        { u"dlt"_s, Logging::isDltEnabled() },
        { u"rules"_s, Logging::filterRules() },
        { u"useAMConsoleLogger"_s, Logging::useAMConsoleLogger() }
    };

    if (Logging::isDltEnabled())
        loggingConfig.insert(u"dltLongMessageBehavior"_s, Logging::dltLongMessageBehavior());

    QVariantMap uiConfig;
    if (m_slowAnimations)
        uiConfig.insert(u"slowAnimations"_s, true);

    QVariantMap openGLConfig;
    if (m_app)
        openGLConfig = m_app->info()->openGLConfiguration();
    if (openGLConfig.isEmpty())
        openGLConfig = manager()->systemOpenGLConfiguration();
    if (!openGLConfig.isEmpty())
        uiConfig.insert(u"opengl"_s, openGLConfig);

    QString iconThemeName = manager()->iconThemeName();
    QStringList iconThemeSearchPaths = manager()->iconThemeSearchPaths();
    if (!iconThemeName.isEmpty())
        uiConfig.insert(u"iconThemeName"_s, iconThemeName);
    if (!iconThemeSearchPaths.isEmpty())
        uiConfig.insert(u"iconThemeSearchPaths"_s, iconThemeSearchPaths);

    QVariantMap config = {
        { u"logging"_s, loggingConfig },
        { u"baseDir"_s, QDir::currentPath() },
        { u"runtimeConfiguration"_s, configuration() },
        { u"securityToken"_s, QString::fromLatin1(securityToken().toHex()) },
        { u"dbus"_s, dbusConfig }
    };

    if (!m_startedViaLauncher && !m_isQuickLauncher)
        config.insert(u"systemProperties"_s, systemProperties());
    if (m_app)
        config.insert(u"application"_s, convertFromJSVariant(m_app->info()->toVariantMap()).toMap());
    if (!uiConfig.isEmpty())
        config.insert(u"ui"_s, uiConfig);

    QMap<QString, QString> env = {
        { u"QT_QPA_PLATFORM"_s, u"wayland"_s },
        { u"QT_IM_MODULE"_s, QString() },     // Applications should use wayland text input
        { u"QT_SCALE_FACTOR"_s, QString() },  // do not scale wayland clients
        { u"AM_CONFIG"_s, QString::fromUtf8(QtYaml::yamlFromVariantDocuments({ config })) },
        { u"QT_WAYLAND_SHELL_INTEGRATION"_s, u"xdg-shell"_s},
    };

    for (const auto *var : {
         "AM_STARTUP_TIMER", "AM_NO_CUSTOM_LOGGING", "AM_NO_CRASH_HANDLER", "AM_FORCE_COLOR_OUTPUT",
         "AM_TIMEOUT_FACTOR", "QT_MESSAGE_PATTERN", "ASAN_OPTIONS", "LSAN_OPTIONS", "TSAN_OPTIONS" }) {
        if (qEnvironmentVariableIsSet(var))
            env.insert(QString::fromLatin1(var), qEnvironmentVariable(var));
    }

    if (!Logging::isDltEnabled()) {
        // we need this to disable DLT as soon as possible
        env.insert(u"AM_NO_DLT_LOGGING"_s, u"1"_s);
    }

    const auto envVars = configuration().value(u"environmentVariables"_s).toMap();
    for (auto it = envVars.cbegin(); it != envVars.cend(); ++it) {
        if (!it.key().isEmpty())
            env.insert(it.key(), it.value().toString());
    }

    if (m_app) {
        const auto appEnvVars = m_app->runtimeParameters().value(u"environmentVariables"_s).toMap();

        if (!appEnvVars.isEmpty()) {
            if (ApplicationManager::instance()->securityChecksEnabled()) {
                qCWarning(LogSystem) << "Due to enabled security checks, the environmentVariables for"
                                     << m_app->id() << "(given in info.yaml) will be ignored";
            } else {
                for (auto it = appEnvVars.cbegin(); it != appEnvVars.cend(); ++it) {
                    if (!it.key().isEmpty())
                        env.insert(it.key(), it.value().toString());
                }
            }
        }
    }

    QStringList args;

    if (!m_startedViaLauncher) {
        args.append(variantToStringList(m_app->runtimeParameters().value(u"arguments"_s)));

        if (!m_document.isNull())
            args << u"--start-argument"_s << m_document;

        if (!Logging::isDltEnabled())
            args << u"--no-dlt-logging"_s;
    } else {
        if (m_isQuickLauncher)
            args << u"--quicklaunch"_s;

        args << QString::fromLocal8Bit(ProcessTitle::placeholderArgument);    // must be last argument
    }

    emit signaler()->aboutToStart(this);

    m_process = m_container->start(args, env, config);

    if (!m_process)
        return false;

    QObject::connect(m_process, &AbstractContainerProcess::started,
                     this, &NativeRuntime::onProcessStarted);
    QObject::connect(m_process, &AbstractContainerProcess::errorOccured,
                     this, &NativeRuntime::onProcessError);
    QObject::connect(m_process, &AbstractContainerProcess::finished,
                     this, &NativeRuntime::onProcessFinished);

    setState(Am::StartingUp);
    return true;
}

void NativeRuntime::stop(bool forceKill)
{
    if (!m_process)
        return;

    setState(Am::ShuttingDown);
    emit aboutToStop();

    if (forceKill) {
        m_process->kill();
    } else if (!m_connectedToApplicationInterface) {
        //The launcher didn't connected to the ApplicationInterface yet, so it won't get the quit signal
        m_process->terminate();
    } else {
        bool ok;
        int qt = configuration().value(u"quitTime"_s).toInt(&ok);
        if (!ok || qt < 0)
            qt = 250;
        QTimer::singleShot(qt, this, [this]() {
            m_process->terminate();
        });
    }
}

void NativeRuntime::onProcessStarted()
{
    if (!m_startedViaLauncher
            && !(application()->info()->supportsApplicationInterface() || manager()->supportsQuickLaunch())) {
        setState(Am::Running);
    }
}

void NativeRuntime::onProcessError(Am::ProcessError error)
{
    Q_UNUSED(error)
    if (m_state != Am::Running && m_state != Am::ShuttingDown)
        shutdown(-1, Am::CrashExit);
}

void NativeRuntime::onProcessFinished(int exitCode, Am::ExitStatus status)
{
    shutdown(exitCode, status);
}

void NativeRuntime::onDBusPeerConnection(const QDBusConnection &connection)
{
    // We have a valid connection - ignore all further connection attempts
    if (m_dbusConnection)
        return;

    m_dbusConnection = true;
    m_dbusConnectionName = connection.name();
    QDBusConnection conn = connection;

    if (!m_dbusApplicationInterface->registerOnDBus(conn, u"/ApplicationInterface"_s)) {
        qCWarning(LogSystem) << "ERROR: could not register the /ApplicationInterface object on the peer DBus:"
                             << conn.lastError().message();
    }

#ifdef EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS
    m_dbusApplicationInterface->registerOnDBus(QDBusConnection::sessionBus(), u"/Application%1/ApplicationInterface"_s.arg(applicationProcessId()));
#endif

    if (m_startedViaLauncher) {
        if (!m_dbusRuntimeInterface->registerOnDBus(conn, u"/RuntimeInterface"_s)) {
            qCWarning(LogSystem) << "ERROR: could not register the /RuntimeInterface object on the peer DBus:"
                                 << conn.lastError().message();
        }

#ifdef EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS
        m_dbusRuntimeInterface->registerOnDBus(QDBusConnection::sessionBus(), u"/Application%1/RuntimeInterface"_s.arg(applicationProcessId()));
#endif
    }
    // the server side of the p2p bus can be setup now, but the client is not able service any
    // requests just yet - only after emitting applicationReadyOnPeerDBus()
    emit applicationConnectedToPeerDBus(conn, m_app);
}

void NativeRuntime::applicationFinishedInitialization()
{
    m_connectedToApplicationInterface = true;

    if (m_app) {
        // now we know which app was launched, so initialize any additional interfaces on the p2p bus
        emit applicationReadyOnPeerDBus(QDBusConnection(m_dbusConnectionName), m_app);

        if (m_startedViaLauncher && m_dbusRuntimeInterface->isRegistered())
            startApplicationViaLauncher();

        setState(Am::Running);
    }
}

bool NativeRuntime::startApplicationViaLauncher()
{
    if (!m_startedViaLauncher || !m_dbusRuntimeInterface->isRegistered() || !m_app)
        return false;

    QString baseDir = m_container->mapHostPathToContainer(m_app->codeDir());
    QString pathInContainer = m_container->mapHostPathToContainer(m_app->info()->absoluteCodeFilePath());

    emit m_dbusRuntimeInterface->generatedAdaptor<RuntimeInterfaceAdaptor>()
        ->startApplication(baseDir, pathInContainer, m_document, m_mimeType,
                           convertFromJSVariant(m_app->info()->toVariantMap()).toMap(),
                           convertFromJSVariant(systemProperties()).toMap());
    return true;
}

qint64 NativeRuntime::applicationProcessId() const
{
    return m_process ? m_process->processId() : 0;
}

void NativeRuntime::openDocument(const QString &document, const QString &mimeType)
{
   m_document = document;
   m_mimeType = mimeType;
   if (m_dbusApplicationInterface->isRegistered()) {
        emit m_dbusApplicationInterface->generatedAdaptor<ApplicationInterfaceAdaptor>()
            ->openDocument(document, mimeType);
   }
}

void NativeRuntime::setSlowAnimations(bool slow)
{
    if (m_slowAnimations != slow) {
        m_slowAnimations = slow;
        if (m_dbusApplicationInterface->isRegistered()) {
            emit m_dbusApplicationInterface->generatedAdaptor<ApplicationInterfaceAdaptor>()
                ->slowAnimationsChanged(slow);
        }
    }
}


NativeRuntimeManager::NativeRuntimeManager(QObject *parent)
    : NativeRuntimeManager(defaultIdentifier(), parent)
{ }

NativeRuntimeManager::NativeRuntimeManager(const QString &id, QObject *parent)
    : AbstractRuntimeManager(id, parent)
{ }

QString NativeRuntimeManager::defaultIdentifier()
{
    return u"native"_s;
}

bool NativeRuntimeManager::supportsQuickLaunch() const
{
    return identifier() != u"native";
}

AbstractRuntime *NativeRuntimeManager::create(AbstractContainer *container, Application *app)
{
    if (!container)
        return nullptr;
    std::unique_ptr<NativeRuntime> nrt(new NativeRuntime(container, app, this));
    if (!nrt || !nrt->initialize())
        return nullptr;

    return nrt.release();
}

QT_END_NAMESPACE_AM

#include "moc_nativeruntime.cpp"
