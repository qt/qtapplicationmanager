/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#if defined(AM_MULTI_PROCESS)
#  include <QDBusMessage>
#  include <QDBusConnection>
#  include <QDBusPendingCallWatcher>
#  include <QDBusPendingReply>

#  include "io.qt.applicationmanager.intentinterface_adaptor.h"
#  include "dbus-utilities.h"
#  include "nativeruntime.h"
#endif
#include <QDebug>
#include <QTimer>

#include "logging.h"
#include "runtimefactory.h"
#include "intentserver.h"
#include "intentclient.h"
#include "intentserverrequest.h"
#include "intentclientrequest.h"
#include "intentaminterface.h"
#include "qmlinprocessruntime.h"
#include "application.h"
#include "applicationmanager.h"

QT_BEGIN_NAMESPACE_AM

//////////////////////////////////////////////////////////////////////////
// vvv IntentAMImplementation vvv


IntentServer *IntentAMImplementation::createIntentServerAndClientInstance()
{
    auto intentServerAMInterface = new IntentServerAMImplementation;
    auto intentClientAMInterface = new IntentClientAMImplementation(intentServerAMInterface);
    auto intentServer = IntentServer::createInstance(intentServerAMInterface);
    auto intentClient = IntentClient::createInstance(intentClientAMInterface);

    // this way, deleting the server (the return value of this factory function) will get rid
    // of both client and server as well as both their AM interfaces
    intentClient->setParent(intentServer);
    return intentServer;
}


// ^^^ IntentAMImplementation ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentServerAMImplementation vvv


void IntentServerAMImplementation::setIntentClientSystemInterface(IntentClientSystemInterface *iface)
{
    m_icsi = iface;
}

IntentClientSystemInterface *IntentServerAMImplementation::intentClientSystemInterface() const
{
    return m_icsi;
}

void IntentServerAMImplementation::initialize(IntentServer *intentManager)
{
    IntentServerSystemInterface::initialize(intentManager);

    // The IntentManager itself doesn't know about the p2p D-Bus or the AM itself, so we need to
    // wire it up to both interfaces from the outside
    connect(&ApplicationManager::instance()->internalSignals, &ApplicationManagerInternalSignals::newRuntimeCreated,
            intentServer(), [this](AbstractRuntime *runtime) {
#if defined(AM_MULTI_PROCESS)
        if (NativeRuntime *nativeRuntime = qobject_cast<NativeRuntime *>(runtime)) {
            connect(nativeRuntime, &NativeRuntime::applicationConnectedToPeerDBus,
                    intentServer(), [this](const QDBusConnection &connection, Application *application) {
                qCDebug(LogIntents) << "IntentManager: applicationConnectedToPeerDBus"
                                    << (application ? application->id() : qSL("<launcher>"));

                IntentServerDBusIpcConnection::create(connection, application, this);
            });

            connect(nativeRuntime, &NativeRuntime::applicationReadyOnPeerDBus,
                    intentServer(), [](const QDBusConnection &connection, Application *application) {
                auto peer = IntentServerDBusIpcConnection::find(connection);

                if (!peer) {
                    qCWarning(LogIntents) << "IntentManager: applicationReadyOnPeerDBus() was emitted, "
                                             "but no previous applicationConnectedToPeerDBus() was seen";
                    return;
                }
                peer->setReady(application);
            });

            connect(nativeRuntime, &NativeRuntime::applicationDisconnectedFromPeerDBus,
                    intentServer(), [](const QDBusConnection &connection, Application *) {
                delete IntentServerDBusIpcConnection::find(connection);
            });
        } else
#endif // defined(AM_MULTI_PROCESS)
        if (QmlInProcessRuntime *qmlRuntime = qobject_cast<QmlInProcessRuntime *>(runtime)) {
            connect(qmlRuntime, &QmlInProcessRuntime::stateChanged,
                    intentServer(), [this, qmlRuntime](Am::RunState newState) {
                if (newState == Am::Running)
                    IntentServerInProcessIpcConnection::create(qmlRuntime->application(), this);
                else if (newState == Am::NotRunning)
                    delete IntentServerIpcConnection::find(qmlRuntime->application()->id());
            });
        }
    });
}

bool IntentServerAMImplementation::checkApplicationCapabilities(const QString &applicationId,
                                                                const QStringList &requiredCapabilities)
{
    const auto app = ApplicationManager::instance()->application(applicationId);
    if (!app)
        return false;

    auto capabilities = app->capabilities();
    for (auto cap : requiredCapabilities) {
        if (!capabilities.contains(cap))
            return false;
    }
    return true;
}

IntentServerSystemInterface::IpcConnection *IntentServerAMImplementation::findClientIpc(const QString &appId)
{
    const auto app = ApplicationManager::instance()->application(appId);
    if (!app)
        return nullptr;
    auto peer = IntentServerIpcConnection::find(appId);
    return (peer && peer->isReady()) ? reinterpret_cast<IpcConnection *>(peer) : nullptr;
}

void IntentServerAMImplementation::startApplication(const QString &appId)
{
    ApplicationManager::instance()->startApplication(appId);
}

void IntentServerAMImplementation::requestToApplication(IntentServerSystemInterface::IpcConnection *clientIPC,
                                                        IntentServerRequest *irs)
{
    reinterpret_cast<IntentServerIpcConnection *>(clientIPC)->requestToApplication(irs);
}

void IntentServerAMImplementation::replyFromSystem(IntentServerSystemInterface::IpcConnection *clientIPC,
                                                   IntentServerRequest *irs)
{
    reinterpret_cast<IntentServerIpcConnection *>(clientIPC)->replyFromSystem(irs);
}


// ^^^ IntentServerAMImplementation ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentClientAMImplementation vvv


IntentClientAMImplementation::IntentClientAMImplementation(IntentServerAMImplementation *serverInterface)
    : IntentClientSystemInterface()
    , m_issi(serverInterface)
{
    serverInterface->setIntentClientSystemInterface(this);
}

void IntentClientAMImplementation::initialize(IntentClient *intentClient) Q_DECL_NOEXCEPT_EXPR(false)
{
    IntentClientSystemInterface::initialize(intentClient);
}

void IntentClientAMImplementation::requestToSystem(IntentClientRequest *icr)
{
    IntentServerRequest *isr = m_issi->requestToSystem(icr->requestingApplicationId(), icr->intentId(),
                                                       icr->applicationId(), icr->parameters());

    QUuid requestId = isr ? isr->id() : QUuid();

    QTimer::singleShot(0, m_ic, [icr, requestId, this]() {
        emit requestToSystemFinished(icr, requestId, requestId.isNull(),
                                     requestId.isNull() ? qSL("Failed") : QString());
    });
}

void IntentClientAMImplementation::replyFromApplication(IntentClientRequest *icr)
{
    emit m_issi->replyFromApplication(icr->applicationId(), icr->requestId(), !icr->succeeded(),
                                      icr->result());
}


// ^^^ IntentClientAMImplementation ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentServerIpcConnection vvv


QList<IntentServerIpcConnection *> IntentServerIpcConnection::s_allPeers;

IntentServerIpcConnection::IntentServerIpcConnection(bool inProcess, Application *application,
                                                     IntentServerAMImplementation *iface)
    : QObject()
    , m_application(application)
    , m_interface(iface)
    , m_inprocess(inProcess)
{
    connect(this, &IntentServerIpcConnection::applicationIsReady,
            m_interface, &IntentServerSystemInterface::applicationWasStarted);
}

IntentServerIpcConnection::~IntentServerIpcConnection()
{ }

bool IntentServerIpcConnection::isReady() const
{
    return m_ready;
}

void IntentServerIpcConnection::setReady(Application *application)
{
    if (m_ready)
        return;
    m_application = application;
    m_ready = true;
    emit applicationIsReady(application->id());
}

IntentServerIpcConnection *IntentServerIpcConnection::find(const QString &appId)
{
    for (auto peer : qAsConst(s_allPeers)) {
        if (peer->m_application->id() == appId)
            return peer;
    }
    return nullptr;
}


Application *IntentServerIpcConnection::application() const
{
    return m_application;
}

bool IntentServerIpcConnection::isInProcess() const
{
    return m_inprocess;
}


// ^^^ IntentServerIpcConnection ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentServerInProcessIpcConnection vvv


IntentServerInProcessIpcConnection::IntentServerInProcessIpcConnection(Application *application,
                                                                       IntentServerAMImplementation *iface)
    : IntentServerIpcConnection(true /*inProcess*/, application, iface)
{ }

IntentServerInProcessIpcConnection::~IntentServerInProcessIpcConnection()
{ }

IntentServerInProcessIpcConnection *IntentServerInProcessIpcConnection::create(Application *application,
                                                                               IntentServerAMImplementation *iface)
{
    auto peer = new IntentServerInProcessIpcConnection(application, iface);
    QTimer::singleShot(0, peer, [peer, application]() { peer->setReady(application); });
    s_allPeers << peer;
    return peer;
}

void IntentServerInProcessIpcConnection::requestToApplication(IntentServerRequest *irs)
{
    // we need decouple the server/client interface at this point to have a consistent
    // behavior in single- and multi-process mode
    QTimer::singleShot(0, this, [this, irs]() {
        auto clientInterface = m_interface->intentClientSystemInterface();
        emit clientInterface->requestToApplication(irs->id().toString(), irs->intent()->id(),
                                                   irs->intent()->applicationId(), irs->parameters());
    });
}

void IntentServerInProcessIpcConnection::replyFromSystem(IntentServerRequest *irs)
{
    // we need decouple the server/client interface at this point to have a consistent
    // behavior in single- and multi-process mode
    QTimer::singleShot(0, this, [this, irs]() {
        auto clientInterface = m_interface->intentClientSystemInterface();
        emit clientInterface->replyFromSystem(irs->id().toString(), !irs->hasSucceeded(), irs->result());
    });
}


// ^^^ IntentServerInProcessIpcConnection ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentServerDBusIpcConnection vvv

#if defined(AM_MULTI_PROCESS)

IntentServerDBusIpcConnection::IntentServerDBusIpcConnection(QDBusConnection connection,
                                                             Application *application,
                                                             IntentServerAMImplementation *iface)
    : IntentServerIpcConnection(false /*!inProcess*/, application, iface)
{
    m_connectionName = connection.name();
    m_adaptor = new IntentInterfaceAdaptor(this);
    connection.registerObject(qSL("/IntentManager"), this, QDBusConnection::ExportAdaptors);
}

IntentServerDBusIpcConnection::~IntentServerDBusIpcConnection()
{
    QDBusConnection(m_connectionName).unregisterObject(qSL("/IntentManager"));
    s_allPeers.removeOne(this);
}

IntentServerDBusIpcConnection *IntentServerDBusIpcConnection::create(QDBusConnection connection,
                                                                     Application *application,
                                                                     IntentServerAMImplementation *iface)
{
    auto peer = new IntentServerDBusIpcConnection(connection, application, iface);
    s_allPeers << peer;
    return peer;
}

IntentServerDBusIpcConnection *IntentServerDBusIpcConnection::find(QDBusConnection connection)
{
    QString connectionName = connection.name();

    for (auto peer : qAsConst(s_allPeers)) {
        if (peer->isInProcess())
            continue;
        auto dbusPeer = static_cast<IntentServerDBusIpcConnection *>(peer);
        if (dbusPeer->m_connectionName == connectionName)
            return dbusPeer;
    }
    return nullptr;
}

void IntentServerDBusIpcConnection::requestToApplication(IntentServerRequest *irs)
{
    emit m_adaptor->requestToApplication(irs->id().toString(), irs->intent()->id(),
                                         irs->intent()->applicationId(),
                                         convertFromJSVariant(irs->parameters()).toMap());
}

void IntentServerDBusIpcConnection::replyFromSystem(IntentServerRequest *irs)
{
    emit m_adaptor->replyFromSystem(irs->id().toString(), !irs->hasSucceeded(),
                                    convertFromJSVariant(irs->result()).toMap());
}

QString IntentServerDBusIpcConnection::requestToSystem(const QString &intentId,
                                                       const QString &applicationId,
                                                       const QVariantMap &parameters)
{
    auto requestingApplicationId = application() ? application()->id() : QString();
    auto irs = m_interface->requestToSystem(requestingApplicationId, intentId, applicationId,
                                            convertFromDBusVariant(parameters).toMap());
    return irs ? irs->id().toString() : QString();
}

void IntentServerDBusIpcConnection::replyFromApplication(const QString &requestId, bool error,
                                                         const QVariantMap &result)
{
    emit m_interface->replyFromApplication(application()->id(), requestId, error,
                                           convertFromDBusVariant(result).toMap());
}

#endif // defined(AM_MULTI_PROCESS)

QT_END_NAMESPACE_AM


// ^^^ IntentServerDBusIpcConnection ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentInterfaceAdaptor vvv

#if defined(AM_MULTI_PROCESS)

IntentInterfaceAdaptor::IntentInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{ }

IntentInterfaceAdaptor::~IntentInterfaceAdaptor()
{ }

void IntentInterfaceAdaptor::replyFromApplication(const QString &requestId, bool error,
                                                  const QVariantMap &result)
{
    auto peer = static_cast<QtAM::IntentServerDBusIpcConnection *>(parent());
    peer->replyFromApplication(requestId, error, result);
}

QString IntentInterfaceAdaptor::requestToSystem(const QString &intentId, const QString &applicationId,
                                                const QVariantMap &parameters)
{
    auto peer = static_cast<QtAM::IntentServerDBusIpcConnection *>(parent());
    return peer->requestToSystem(intentId, applicationId, parameters);
}

#endif // defined(AM_MULTI_PROCESS)

// ^^^ IntentInterfaceAdaptor ^^^
//////////////////////////////////////////////////////////////////////////
