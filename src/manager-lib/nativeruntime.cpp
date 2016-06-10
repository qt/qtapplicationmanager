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

#include <QCoreApplication>
#include <QProcess>
#include <QDBusServer>
#include <QDBusConnection>
#include <QDBusError>

#include "global.h"
#include "application.h"
#include "applicationmanager.h"
#include "nativeruntime.h"
#include "nativeruntime_p.h"
#include "runtimefactory.h"
#include "qtyaml.h"
#include "applicationinterface.h"
#include "applicationipcmanager.h"
#include "applicationipcinterface.h"

// You can enable this define to get all P2P-bus objects onto the session bus
// within io.qt.ApplicationManager, /Application<pid>/...

// #define EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS 1


#if defined(AM_MULTI_PROCESS)
#  include <dbus/dbus.h>
#  include <sys/socket.h>

static qint64 getDBusPeerPid(const QDBusConnection &conn)
{
    int socketFd = -1;
    if (dbus_connection_get_socket(static_cast<DBusConnection *>(conn.internalPointer()), &socketFd)) {
        struct ucred ucred;
        socklen_t ucredSize = sizeof(struct ucred);
        if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucredSize) == 0)
            return ucred.pid;
    }
    return 0;
}

#else

static qint64 getDBusPeerPid(const QDBusConnection &conn)
{
    Q_UNUSED(conn)
    return 0;
}

#endif


NativeRuntime::NativeRuntime(AbstractContainer *container, const Application *app, NativeRuntimeManager *manager)
    : AbstractRuntime(container, app, manager)
    , m_isQuickLauncher(app == nullptr)
    , m_needsLauncher(manager->identifier() != qL1S("native"))
{ }

NativeRuntime::~NativeRuntime()
{
    delete m_process;
}

bool NativeRuntime::isQuickLauncher() const
{
    return m_isQuickLauncher;
}

bool NativeRuntime::attachApplicationToQuickLauncher(const Application *app)
{
    if (!app || !isQuickLauncher() || !m_needsLauncher)
        return false;

    m_isQuickLauncher = false;
    m_app = app;
    m_app->setCurrentRuntime(this);

    onLauncherFinishedInitialization();
    return m_launched;
}

bool NativeRuntime::initialize()
{
    if (m_needsLauncher) {
        QFileInfo fi(qSL("%1/appman-launcher-%2").arg(QCoreApplication::applicationDirPath(), manager()->identifier()));
        if (!fi.exists() || !fi.isExecutable())
            return false;
        m_container->setProgram(fi.absoluteFilePath());
        m_container->setBaseDirectory(fi.absolutePath());
    } else {
        if (!m_app)
            return false;

        m_container->setProgram(m_app->absoluteCodeFilePath());
        m_container->setBaseDirectory(m_app->baseDir().absolutePath());
    }
    return true;
}

void NativeRuntime::shutdown(int exitCode, QProcess::ExitStatus status)
{
    qCDebug(LogSystem) << "NativeRuntime (id:" << (m_app ? m_app->id() : qSL("(none)"))
                       << "pid:" << m_process->processId() << ") exited with code:" << exitCode
                       << "status:" << status;

    m_shutingDown = m_launched = m_launchWhenReady = m_started = m_dbusConnection = false;

    // unregister all extension interfaces
    foreach (ApplicationIPCInterface *iface, ApplicationIPCManager::instance()->interfaces()) {
        iface->dbusUnregister(QDBusConnection(m_dbusConnectionName));
    }

    emit finished(exitCode, status);
    emit stateChanged(state());

    if (m_app)
        m_app->setCurrentRuntime(0);
    deleteLater();
}

bool NativeRuntime::start()
{
    switch (state()) {
    case Startup:
    case Active:
        return true;
    case Shutdown:
        return false;
    case Inactive:
        break;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(qSL("QT_QPA_PLATFORM"), qSL("wayland"));
    //env.insert(qSL("QT_WAYLAND_DISABLE_WINDOWDECORATION"), "1");
    env.insert(qSL("AM_SECURITY_TOKEN"), qL1S(securityToken().toHex()));
    env.insert(qSL("AM_DBUS_PEER_ADDRESS"), static_cast<NativeRuntimeManager *>(manager())->applicationInterfaceServer()->address());
    env.insert(qSL("AM_RUNTIME_CONFIGURATION"), QtYaml::yamlFromVariantDocuments({ configuration() }));
    env.insert(qSL("AM_RUNTIME_ADDITIONAL_CONFIGURATION"), QtYaml::yamlFromVariantDocuments({ additionalConfiguration() }));
    env.insert(qSL("AM_BASE_DIR"), QDir::currentPath());

    for (QMapIterator<QString, QVariant> it(configuration().value(qSL("environmentVariables")).toMap()); it.hasNext(); ) {
        it.next();
        QString name = it.key();
        if (!name.isEmpty()) {
            QString value = it.value().toString();
            if (value.isEmpty())
                env.remove(it.key());
            else
                env.insert(name, value);
        }
    }

    QStringList args;

    if (m_needsLauncher) {
        m_launchWhenReady = true;
    } else {
        if (!m_document.isNull())
            args << qSL("--start-argument") << m_document;
    }
    m_process = m_container->start(args, env);

    if (!m_process)
        return false;

    QObject::connect(m_process, &AbstractContainerProcess::started,
                     this, &NativeRuntime::onProcessStarted);
    QObject::connect(m_process, &AbstractContainerProcess::error,
                     this, &NativeRuntime::onProcessError);
    QObject::connect(m_process, &AbstractContainerProcess::finished,
                     this, &NativeRuntime::onProcessFinished);

    emit stateChanged(state());
    return true;
}

void NativeRuntime::stop(bool forceKill)
{
    if (!m_process)
        return;

    m_shutingDown = true;

    emit aboutToStop();
    emit stateChanged(state());

    if (forceKill)
        m_process->kill();
    else
        m_process->terminate();
}

void NativeRuntime::onProcessStarted()
{
    m_started = true;
    emit stateChanged(state());
}

void NativeRuntime::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    if (!m_started)
        shutdown(-1, QProcess::CrashExit);
}

void NativeRuntime::onProcessFinished(int exitCode, QProcess::ExitStatus status)
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

    m_applicationInterface = new NativeRuntimeApplicationInterface(this);
    if (!conn.registerObject(qSL("/ApplicationInterface"), m_applicationInterface, QDBusConnection::ExportScriptableContents))
        qCWarning(LogSystem) << "ERROR: could not register the /ApplicationInterface object on the peer DBus:" << conn.lastError().name() << conn.lastError().message();

#ifdef EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS
    QDBusConnection::sessionBus().registerObject(qSL("/Application%1/ApplicationInterface").arg(applicationProcessId()),
                                                 m_applicationInterface, QDBusConnection::ExportScriptableContents);
#endif

    if (m_needsLauncher && m_launchWhenReady && !m_launched) {
        m_runtimeInterface = new NativeRuntimeInterface(this);
        if (!conn.registerObject(qSL("/RuntimeInterface"), m_runtimeInterface, QDBusConnection::ExportScriptableContents))
            qCWarning(LogSystem) << "ERROR: could not register the /RuntimeInterface object on the peer DBus.";

#ifdef EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS
        QDBusConnection::sessionBus().registerObject(qSL("/Application%1/RuntimeInterface").arg(applicationProcessId()),
                                                     m_runtimeInterface, QDBusConnection::ExportScriptableContents);
#endif
        // we need to delay the actual start call, until the launcher side is ready to
        // listen to the interface
        connect(m_runtimeInterface, &NativeRuntimeInterface::launcherFinishedInitialization,
                this, &NativeRuntime::onLauncherFinishedInitialization);
    }
}

void NativeRuntime::onLauncherFinishedInitialization()
{
    if (m_needsLauncher && m_launchWhenReady && !m_launched && m_app && m_runtimeInterface) {
        // register all extension interfaces
        QDBusConnection conn(m_dbusConnectionName);

        foreach (ApplicationIPCInterface *iface, ApplicationIPCManager::instance()->interfaces()) {
            if (!iface->isValidForApplication(application()))
                continue;

            if (!iface->dbusRegister(application(), conn)) {
                qCWarning(LogSystem) << "ERROR: could not register the application IPC interface" << iface->interfaceName()
                                     << "at" << iface->pathName() << "on the peer DBus:" << conn.lastError().name() << conn.lastError().message();
            }
#ifdef EXPORT_P2PBUS_OBJECTS_TO_SESSION_BUS
            iface->dbusRegister(QDBusConnection::sessionBus(), qSL("/Application%1").arg(applicationProcessId()));
#endif
        }

        QString pathInContainer = m_container->mapHostPathToContainer(m_app->absoluteCodeFilePath());
        m_runtimeInterface->startApplication(pathInContainer, m_document, m_app->runtimeParameters());
        m_launched = true;
    }
}


AbstractRuntime::State NativeRuntime::state() const
{
    if (m_process) {
        if (m_process->state() == QProcess::Starting)
            return Startup;
        if (m_process->state() == QProcess::Running)
            return m_shutingDown ? Shutdown : Active;
    }
    return Inactive;
}

qint64 NativeRuntime::applicationProcessId() const
{
    return m_process ? m_process->processId() : 0;
}

void NativeRuntime::openDocument(const QString &document)
{
   m_document = document;
   if (m_applicationInterface)
       m_applicationInterface->openDocument(document);
}


NativeRuntimeApplicationInterface::NativeRuntimeApplicationInterface(NativeRuntime *runtime)
    : ApplicationInterface(runtime)
    , m_runtime(runtime)
{
    connect(ApplicationManager::instance(), &ApplicationManager::memoryLowWarning,
            this, &ApplicationInterface::memoryLowWarning);
    connect(runtime, &NativeRuntime::aboutToStop,
            this, &ApplicationInterface::quit);
}


QString NativeRuntimeApplicationInterface::applicationId() const
{
    if (m_runtime->application())
        return m_runtime->application()->id();
    return QString();
}

QVariantMap NativeRuntimeApplicationInterface::additionalConfiguration() const
{
    if (m_runtime)
        return m_runtime->additionalConfiguration();
    return QVariantMap();
}

NativeRuntimeInterface::NativeRuntimeInterface(NativeRuntime *runtime)
    : QObject(runtime)
    , m_runtime(runtime)
{ }

void NativeRuntimeInterface::finishedInitialization()
{
    emit launcherFinishedInitialization();
}


NativeRuntimeManager::NativeRuntimeManager(const QString &id, QObject *parent)
    : AbstractRuntimeManager(id, parent)
    , m_applicationInterfaceServer(new QDBusServer(qSL("unix:tmpdir=/tmp")))
{
    connect(m_applicationInterfaceServer, &QDBusServer::newConnection,
            this, [this](const QDBusConnection &connection) {
        // If multiple apps are starting in parallel, there will be multiple NativeRuntime objects
        // listening to the newConnection signal from the one and only DBusServer.
        // We have to make sure to forward to the correct runtime object while also handling
        // error conditions.

        qint64 pid = getDBusPeerPid(connection);
        if (pid <= 0) {
            QDBusConnection::disconnectFromPeer(connection.name());
            qCWarning(LogSystem) << "Could not retrieve peer pid on D-Bus connection attempt.";
            return;
        }
        for (NativeRuntime *rt : m_nativeRuntimes) {
            if (rt->applicationProcessId() == pid) {
                rt->onDBusPeerConnection(connection);
                return;
            }
        }
        QDBusConnection::disconnectFromPeer(connection.name());
        qCWarning(LogSystem) << "Connection attempt on peer D-Bus from unknown pid:" << pid;
    });
}

QString NativeRuntimeManager::defaultIdentifier()
{
    return qSL("native");
}

bool NativeRuntimeManager::supportsQuickLaunch() const
{
    return identifier() != qL1S("native");
}

AbstractRuntime *NativeRuntimeManager::create(AbstractContainer *container, const Application *app)
{
    if (!container)
        return nullptr;
    QScopedPointer<NativeRuntime> nrt(new NativeRuntime(container, app, this));
    if (!nrt || !nrt->initialize())
        return nullptr;
    connect(nrt.data(), &QObject::destroyed, this, [this](QObject *o) {
        m_nativeRuntimes.removeOne(static_cast<NativeRuntime *>(o));
    });
    m_nativeRuntimes.append(nrt.data());
    return nrt.take();
}

QDBusServer *NativeRuntimeManager::applicationInterfaceServer() const
{
    return m_applicationInterfaceServer;
}
