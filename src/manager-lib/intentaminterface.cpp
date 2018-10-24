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
#include <QMetaObject>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlContext>
#include <private/qv4engine_p.h>
#include <private/qqmlcontext_p.h>

#include "error.h"
#include "exception.h"
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

static QString sysUiId = qSL(":sysui:");


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

void IntentAMImplementation::addApplicationIntents(AbstractApplication *app, IntentServer *intentServer)
{
    if (app->isAlias())
        return;

    QSet<QString> intentIds;
    const auto intents = app->nonAliasedInfo()->intents();

    if (!intents.isEmpty())
        intentServer->addApplication(app->id());

    for (const auto &intent : intents) {
        /* example:
           id: io.qt.shareImage
           handledBy: main
           visibility: public*|private
           requiredCapabilities: [ a, b ]
           parameterMatch:
             mimeType: "^image/.*\.png$"
        */
        const QVariantMap map = intent.toMap();
        const QString id = map[qSL("id")].toString();
        Intent::Visibility visibility = Intent::Public;
        const QString visibilityStr = map[qSL("visibility")].toString();
        QString handledBy = map[qSL("handledBy")].toString();
        const QStringList capabilities = map[qSL("requiredCapabilities")].toStringList();
        const QVariantMap parameterMatch = map[qSL("parameterMatch")].toMap(); // do we really need that?

        if (id.isEmpty())
            throw Exception(Error::Intents, "intents need to have an id (app %1)").arg(app->id());
        if (intentIds.contains(id))
            throw Exception(Error::Intents, "found two intent handlers for %2 (app %1)").arg(app->id()).arg(id);
        intentIds << id;

        if (visibilityStr == qL1S("private"))
            visibility = Intent::Private;
        else if (visibilityStr == qL1S("hidden"))
            visibility = Intent::Hidden;
        else if (!visibilityStr.isEmpty() && (visibilityStr != qL1S("public"))) {
            throw Exception(Error::Intents, "intent visibilty %3 is invalid (intent %2, app %1)")
                    .arg(app->id()).arg(id).arg(visibilityStr);
        }

        if (handledBy == qL1S("main"))
            handledBy.clear();
        // we do not support bg services yet
        if (!handledBy.isEmpty()) {
            throw Exception(Error::Intents, "service background handlers for intent are not supported yet (intent %2, app %1)")
                    .arg(app->id()).arg(id).arg(visibilityStr);
        }

        qCDebug(LogSystem).nospace().noquote() << " * " << id << " [app: " << app->id() << "]";

        if (!intentServer->addIntent(id, app->id(), handledBy, capabilities,
                                     visibility, parameterMatch)) {
            throw Exception(Error::Intents, "could not add intent %2 for app %1").arg(app->id()).arg(id);
        }
    }
}

void IntentAMImplementation::removeApplicationIntents(AbstractApplication *app, IntentServer *intentServer)
{
    if (app->isAlias())
        return;
    auto intents = intentServer->filterByHandlingApplicationId(intentServer->all(), app->id());
    for (const auto &intent : intents)
        intentServer->removeIntent(intent);

    intentServer->removeApplication(app->id());
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

void IntentServerAMImplementation::initialize(IntentServer *server)
{
    IntentServerSystemInterface::initialize(server);

    // this is a dummy connection for the system-ui, so that we can route replies
    // back to it without any ugly hacks in the core IntentServer code.
    IntentServerInProcessIpcConnection::createSystemUi(this);

    // The IntentServer itself doesn't know about the p2p D-Bus or the AM itself, so we need to
    // wire it up to both interfaces from the outside
    connect(&ApplicationManager::instance()->internalSignals, &ApplicationManagerInternalSignals::newRuntimeCreated,
            intentServer(), [this](AbstractRuntime *runtime) {
#if defined(AM_MULTI_PROCESS)
        if (NativeRuntime *nativeRuntime = qobject_cast<NativeRuntime *>(runtime)) {
            connect(nativeRuntime, &NativeRuntime::applicationConnectedToPeerDBus,
                    intentServer(), [this](const QDBusConnection &connection, Application *application) {
                qCDebug(LogIntents) << "IntentServer: applicationConnectedToPeerDBus"
                                    << (application ? application->id() : qSL("<launcher>"));

                IntentServerDBusIpcConnection::create(connection, application, this);
            });

            connect(nativeRuntime, &NativeRuntime::applicationReadyOnPeerDBus,
                    intentServer(), [](const QDBusConnection &connection, Application *application) {
                auto peer = IntentServerDBusIpcConnection::find(connection);

                if (!peer) {
                    qCWarning(LogIntents) << "IntentServer: applicationReadyOnPeerDBus() was emitted, "
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
                if (!qmlRuntime->application())
                    return;

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
    if (applicationId == sysUiId) // The System-UI bypasses the capabilities check
        return true;

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

QString IntentClientAMImplementation::currentApplicationId()
{
    // IntentClient is a singleton and as such, it is running in its own global QQmlContext, which
    // is independent of the current execution context. Since we want to know the appId of the
    // in-process app that called into the singleton, we need to rely on the callingQmlContext()
    // provided by the v4 engine.

    if (QQmlEngine *engine = qmlEngine(m_ic)) {
        if (QV4::ExecutionEngine *v4 = engine->handle()) {
            if (QQmlContextData *ctxtData = v4->callingQmlContext()) {
                if (QQmlContext *ctxt = ctxtData->asQQmlContext()) {
                    QQmlExpression expr(ctxt, nullptr, qSL("ApplicationInterface.applicationId"), nullptr);
                    QVariant v = expr.evaluate();
                    if (!v.isNull())
                        return v.toString();
                    else
                        return qSL(":sysui:");
                }
            }
        }
    }
    return QString();
}

void IntentClientAMImplementation::initialize(IntentClient *intentClient) Q_DECL_NOEXCEPT_EXPR(false)
{
    IntentClientSystemInterface::initialize(intentClient);
}

void IntentClientAMImplementation::requestToSystem(QPointer<IntentClientRequest> icr)
{
    IntentServerRequest *isr = m_issi->requestToSystem(icr->requestingApplicationId(), icr->intentId(),
                                                       icr->applicationId(), icr->parameters());

    QUuid requestId = isr ? isr->requestId() : QUuid();

    QMetaObject::invokeMethod(m_ic, [icr, requestId, this]() {
        emit requestToSystemFinished(icr.data(), requestId, requestId.isNull(),
                                     requestId.isNull() ? qSL("Failed") : QString());
    }, Qt::QueuedConnection);
}

void IntentClientAMImplementation::replyFromApplication(QPointer<IntentClientRequest> icr)
{
    if (icr) {
        emit m_issi->replyFromApplication(icr->applicationId(), icr->requestId(), !icr->succeeded(),
                                          icr->result());
    }
}


// ^^^ IntentClientAMImplementation ^^^
//////////////////////////////////////////////////////////////////////////
// vvv IntentServerIpcConnection vvv


QList<IntentServerIpcConnection *> IntentServerIpcConnection::s_ipcConnections;

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
{
    s_ipcConnections.removeOne(this);
}

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
    emit applicationIsReady((isInProcess() && !application) ? sysUiId : application->id());
}

IntentServerIpcConnection *IntentServerIpcConnection::find(const QString &appId)
{
    for (auto ipcConnection : qAsConst(s_ipcConnections)) {
        if (ipcConnection->applicationId() == appId)
            return ipcConnection;
    }
    return nullptr;
}

Application *IntentServerIpcConnection::application() const
{
    return m_application;
}

QString IntentServerIpcConnection::applicationId() const
{
    return m_application ? m_application->id() : QString();
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

IntentServerInProcessIpcConnection *IntentServerInProcessIpcConnection::create(Application *application,
                                                                               IntentServerAMImplementation *iface)
{
    auto ipcConnection = new IntentServerInProcessIpcConnection(application, iface);
    QMetaObject::invokeMethod(ipcConnection,
                              [ipcConnection, application]() { ipcConnection->setReady(application); },
                              Qt::QueuedConnection);
    s_ipcConnections << ipcConnection;
    return ipcConnection;
}

IntentServerInProcessIpcConnection *IntentServerInProcessIpcConnection::createSystemUi(IntentServerAMImplementation *iface)
{
    auto ipcConnection = create(nullptr, iface);
    ipcConnection->m_isSystemUi = true;
    return ipcConnection;
}

QString IntentServerInProcessIpcConnection::applicationId() const
{
    return m_isSystemUi ? sysUiId : IntentServerIpcConnection::applicationId();
}

void IntentServerInProcessIpcConnection::requestToApplication(IntentServerRequest *irs)
{
    // we need decouple the server/client interface at this point to have a consistent
    // behavior in single- and multi-process mode
    QMetaObject::invokeMethod(this, [this, irs]() {
        auto clientInterface = m_interface->intentClientSystemInterface();
        emit clientInterface->requestToApplication(irs->requestId().toString(), irs->intentId(),
                                                   irs->handlingApplicationId(), irs->parameters());
    }, Qt::QueuedConnection);
}

void IntentServerInProcessIpcConnection::replyFromSystem(IntentServerRequest *irs)
{
    // we need decouple the server/client interface at this point to have a consistent
    // behavior in single- and multi-process mode
    QMetaObject::invokeMethod(this, [this, irs]() {
        auto clientInterface = m_interface->intentClientSystemInterface();
        emit clientInterface->replyFromSystem(irs->requestId().toString(), !irs->succeeded(), irs->result());
    }, Qt::QueuedConnection);
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
    connection.registerObject(qSL("/IntentServer"), this, QDBusConnection::ExportAdaptors);
}

IntentServerDBusIpcConnection::~IntentServerDBusIpcConnection()
{
    QDBusConnection(m_connectionName).unregisterObject(qSL("/IntentServer"));
}

IntentServerDBusIpcConnection *IntentServerDBusIpcConnection::create(QDBusConnection connection,
                                                                     Application *application,
                                                                     IntentServerAMImplementation *iface)
{
    auto ipcConnection = new IntentServerDBusIpcConnection(connection, application, iface);
    s_ipcConnections << ipcConnection;
    return ipcConnection;
}

IntentServerDBusIpcConnection *IntentServerDBusIpcConnection::find(QDBusConnection connection)
{
    QString connectionName = connection.name();

    for (auto ipcConnection : qAsConst(s_ipcConnections)) {
        if (ipcConnection->isInProcess())
            continue;
        auto dbusIpcConnection = static_cast<IntentServerDBusIpcConnection *>(ipcConnection);
        if (dbusIpcConnection->m_connectionName == connectionName)
            return dbusIpcConnection;
    }
    return nullptr;
}

void IntentServerDBusIpcConnection::requestToApplication(IntentServerRequest *irs)
{
    emit m_adaptor->requestToApplication(irs->requestId().toString(), irs->intentId(),
                                         irs->handlingApplicationId(),
                                         convertFromJSVariant(irs->parameters()).toMap());
}

void IntentServerDBusIpcConnection::replyFromSystem(IntentServerRequest *irs)
{
    emit m_adaptor->replyFromSystem(irs->requestId().toString(), !irs->succeeded(),
                                    convertFromJSVariant(irs->result()).toMap());
}

QString IntentServerDBusIpcConnection::requestToSystem(const QString &intentId,
                                                       const QString &applicationId,
                                                       const QVariantMap &parameters)
{
    auto requestingApplicationId = application() ? application()->id() : QString();
    auto irs = m_interface->requestToSystem(requestingApplicationId, intentId, applicationId,
                                            convertFromDBusVariant(parameters).toMap());
    return irs ? irs->requestId().toString() : QString();
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
