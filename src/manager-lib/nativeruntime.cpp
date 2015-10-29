/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
#include "dbus-utilities.h"
#include "applicationinterface.h"


QDBusServer *NativeRuntime::s_applicationInterfaceServer = 0;


NativeRuntime::NativeRuntime(QObject *parent)
    : AbstractRuntime(parent)
{
    if (!s_applicationInterfaceServer)
        s_applicationInterfaceServer = new QDBusServer(QLatin1String("unix:tmpdir=/tmp"));

    connect(s_applicationInterfaceServer, &QDBusServer::newConnection,
            this, &NativeRuntime::onDBusPeerConnection);
}

QString NativeRuntime::identifier()
{
    return QLatin1String("native");
}

bool NativeRuntime::create(const Application *app)
{
    if (!app)
        return false;
    QString rtName = app->runtimeName();
    if (rtName != QLatin1String("native")) {
        if (rtName.isEmpty())
            return false;
        QFileInfo fi (QString::fromLatin1("%1/appman-launcher-%2").arg(QCoreApplication::applicationDirPath(), rtName));
        if (!fi.exists() || !fi.isExecutable())
            return false;
        m_launcherCommand = fi.absoluteFilePath();
    }
    m_app = app;
    return true;
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
    env.insert("QT_QPA_PLATFORM", "wayland");                               // set wayland as platform plugin
    //env.insert("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");                 // disable (client side) window decorations
    env.insert("AM_SECURITY_TOKEN", securityToken().toHex());
    env.insert("AM_DBUS_PEER_ADDRESS", s_applicationInterfaceServer->address());
    env.insert("AM_RUNTIME_CONFIGURATION", QtYaml::yamlFromVariantDocuments({ configuration() }));
    env.insert("AM_BASE_DIR", QDir::currentPath());

    if (m_launcherCommand.isEmpty()) {
        QStringList args;
        if (!m_document.isNull())
            args << QLatin1String("--start-argument") << m_document;

        m_process = m_container->startApp(args, env);
    } else {
        m_launchWhenReady = true;
        m_process = m_container->startApp(m_launcherCommand, QStringList(), env);
    }

    if (!m_process)
        return false;

    QObject::connect(m_process, &ExecutionContainerProcess::started,
                     this, &NativeRuntime::onProcessStarted);
    QObject::connect(m_process, &ExecutionContainerProcess::error,
                     this, &NativeRuntime::onProcessError);
    QObject::connect(m_process, &ExecutionContainerProcess::finished,
                     this, &NativeRuntime::onProcessFinished);
    return true;
}

void NativeRuntime::stop(bool forceKill)
{
    if (!m_process)
        return;

    m_shutingDown = true;

    emit aboutToStop();

    if (forceKill)
        m_process->kill();
    else
        m_process->terminate();
}

void NativeRuntime::onProcessStarted()
{ }

void NativeRuntime::onProcessError(QProcess::ProcessError error)
{
    qCCritical(LogSystem) << "QmlRuntime (id:" << (m_app ? m_app->id() : QString("(none)")) << "pid:" << m_process->pid() << ") exited with ProcessError: " << error;

    m_process->deleteLater();
    m_process = 0;
    m_shutingDown = m_launched = m_launchWhenReady = m_dbusConnection = false;

    emit finished(-1, QProcess::CrashExit);
}

void NativeRuntime::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_process->deleteLater();
    m_process = 0;
    m_shutingDown = m_launched = m_launchWhenReady = m_dbusConnection = false;

    qCDebug(LogSystem) << "NativeRuntume: process finished" << exitCode << status;

    emit finished(exitCode, status);
}

void NativeRuntime::onDBusPeerConnection(const QDBusConnection &connection)
{
    if (!m_app || m_dbusConnection)
        return;

    // If multiple apps are starting in parallel, there will be multiple NativeRuntime objects
    // listening to the newConnection signal from the one and only DBusServer.
    // We have to make sure that we are only reacting on "our" client's connection attempt.
    Q_PID pid = getDBusPeerPid(connection);
    if (pid != applicationPID())
        return;

    // We have a valid connection - ignore all further connection attempts
    m_dbusConnection = true;

    QDBusConnection conn = connection;

    m_applicationInterface = new NativeRuntimeApplicationInterface(this);
    if (!conn.registerObject("/ApplicationInterface", m_applicationInterface, QDBusConnection::ExportScriptableContents))
        qCWarning(LogSystem) << "ERROR: could not register the /ApplicationInterface object on the peer DBus:" << conn.lastError().name() << conn.lastError().message();

    // Useful for debugging the private P2P bus:
    //QDBusConnection::sessionBus().registerObject(QString::fromLatin1("/Application%1").arg(pid), m_applicationInterface, QDBusConnection::ExportScriptableContents);

    if (!m_launcherCommand.isEmpty() && m_launchWhenReady && !m_launched) {
        m_runtimeInterface = new NativeRuntimeInterface(this);
        if (!conn.registerObject("/RuntimeInterface", m_runtimeInterface, QDBusConnection::ExportScriptableContents))
            qCWarning(LogSystem) << "ERROR: could not register the /RuntimeInterface object on the peer DBus.";

        // we need to delay the actual start call, until the launcher side is ready to
        // listen to the interface
        connect(m_runtimeInterface, &NativeRuntimeInterface::launcherFinishedInitialization,
                this, &NativeRuntime::onLauncherFinishedInitialization);
    }
}

void NativeRuntime::onLauncherFinishedInitialization()
{
    if (!m_launcherCommand.isEmpty() && m_launchWhenReady && !m_launched) {
        QString pathInContainer = m_container->applicationBaseDir().absolutePath() + "/" + m_app->codeFilePath();

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

Q_PID NativeRuntime::applicationPID() const
{
    return m_process ? m_process->pid() : INVALID_PID;
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


NativeRuntimeInterface::NativeRuntimeInterface(NativeRuntime *runtime)
    : QObject(runtime)
    , m_runtime(runtime)
{ }

void NativeRuntimeInterface::finishedInitialization()
{
    emit launcherFinishedInitialization();
}
