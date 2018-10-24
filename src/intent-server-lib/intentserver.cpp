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

#include "intentserver.h"
#include "intentserversysteminterface.h"
#include "intentserverrequest.h"
#include <QtAppManCommon/logging.h>

#include <algorithm>

#include <QRegularExpression>
#include <QUuid>
#include <QMetaObject>
#include <QDebug>

#include <QQmlEngine>
#include <QQmlInfo>


QT_BEGIN_NAMESPACE_AM

IntentServer *IntentServer::s_instance = nullptr;

IntentServer *IntentServer::createInstance(IntentServerSystemInterface *systemInterface)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("IntentServer::createInstance() was called a second time.");
    if (Q_UNLIKELY(!systemInterface))
        qFatal("IntentServer::createInstance() was called without a systemInterface.");

    QScopedPointer<IntentServer> is(new IntentServer(systemInterface));
    systemInterface->initialize(is.data());

    qRegisterMetaType<Intent>("Intent");

    // Have a nicer name in the C++ API, since QML cannot cope with QList<Q_GADGET-type>
    qRegisterMetaType<QVariantList>("IntentList");

    // needed to get access to the Visibility enum from QML
    qmlRegisterUncreatableType<Intent>("QtApplicationManager", 1, 0, "Intent",
                                       qSL("Cannot create objects of type Intent"));

    qmlRegisterUncreatableType<IntentServerRequest>("QtApplicationManager", 1, 0, "IntentServerRequest",
                                                    qSL("Cannot create objects of type IntentServerRequest"));
    qmlRegisterSingletonType<IntentServer>("QtApplicationManager", 1, 0, "IntentServer",
                                           [](QQmlEngine *, QJSEngine *) -> QObject * {
        QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
        return instance();
    });
    return s_instance = is.take();
}

IntentServer *IntentServer::instance()
{
    if (!s_instance)
        qFatal("IntentServer::instance() was called before createInstance().");
    return s_instance;
}

IntentServer::IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent)
    : QObject(parent)
    , m_systemInterface(systemInterface)
{
    m_systemInterface->setParent(this);
    connect(this, &IntentServer::intentAdded, this, &IntentServer::intentListChanged);
    connect(this, &IntentServer::intentRemoved, this, &IntentServer::intentListChanged);
}

IntentServer::~IntentServer()
{
    s_instance = nullptr;
}

bool IntentServer::addApplication(const QString &applicationId)
{
    if (m_knownApplications.contains(applicationId))
        return false;
    m_knownApplications << applicationId;
    return true;
}

void IntentServer::removeApplication(const QString &applicationId)
{
    m_knownBackgroundServices.remove(applicationId);
    m_knownApplications.removeOne(applicationId);
}

bool IntentServer::addApplicationBackgroundHandler(const QString &applicationId, const QString &backgroundServiceId)
{
    if (!m_knownApplications.contains(applicationId))
        return false;
    const QStringList services = m_knownBackgroundServices.value(applicationId);
    if (services.contains(backgroundServiceId))
        return false;
    m_knownBackgroundServices[applicationId].append(backgroundServiceId);
    return true;
}

void IntentServer::removeApplicationBackgroundHandler(const QString &applicationId, const QString &backgroundServiceId)
{
    m_knownBackgroundServices[applicationId].removeAll(backgroundServiceId);
}

Intent IntentServer::addIntent(const QString &id, const QString &applicationId,
                               const QStringList &capabilities, Intent::Visibility visibility,
                               const QVariantMap &parameterMatch)
{
    return addIntent(id, applicationId, QString(), capabilities, visibility, parameterMatch);
}

Intent IntentServer::addIntent(const QString &id, const QString &applicationId,
                               const QString &backgroundHandlerId,
                               const QStringList &capabilities, Intent::Visibility visibility,
                               const QVariantMap &parameterMatch)
{
    if (id.isEmpty()
            || !m_knownApplications.contains(applicationId)
            || find(id, applicationId)
            || (!backgroundHandlerId.isEmpty()
                && !m_knownBackgroundServices[applicationId].contains(backgroundHandlerId))) {
        return Intent();
    }

    auto intent = Intent(id, applicationId, backgroundHandlerId, capabilities, visibility,
                         parameterMatch);
    m_intents << intent;
    emit intentAdded(intent);
    return intent;
}

void IntentServer::removeIntent(const Intent &intent)
{
    int index = m_intents.indexOf(intent);
    if (index >= 0) {
        m_intents.removeAt(index);
        emit intentRemoved(intent);
    }
}

QVector<Intent> IntentServer::all() const
{
    return m_intents;
}

QVector<Intent> IntentServer::filterByIntentId(const QVector<Intent> &intents, const QString &intentId,
                                               const QVariantMap &parameters) const
{
    QVector<Intent> result;
    std::copy_if(intents.cbegin(), intents.cend(), std::back_inserter(result),
                 [intentId, parameters](const Intent &intent) -> bool {
        return (intent.intentId() == intentId) && intent.checkParameterMatch(parameters);

    });
    return result;
}

QVector<Intent> IntentServer::filterByHandlingApplicationId(const QVector<Intent> &intents,
                                                            const QString &handlingApplicationId,
                                                            const QVariantMap &parameters) const
{
    QVector<Intent> result;
    std::copy_if(intents.cbegin(), intents.cend(), std::back_inserter(result),
                 [handlingApplicationId, parameters](const Intent &intent) -> bool {
        return (intent.applicationId() == handlingApplicationId) && intent.checkParameterMatch(parameters);

    });
    return result;
}

QVector<Intent> IntentServer::filterByRequestingApplicationId(const QVector<Intent> &intents,
                                                              const QString &requestingApplicationId) const
{
    QVector<Intent> result;
    std::copy_if(intents.cbegin(), intents.cend(), std::back_inserter(result),
                 [this, requestingApplicationId](const Intent &intent) -> bool {
        // filter on visibility and capabilities, if the requesting app is different from the
        // handling app
        if (intent.applicationId() != requestingApplicationId) {
            if (intent.visibility() == Intent::Private) {
                qCDebug(LogIntents) << "Not considering" << intent.intentId() << "/" << intent.applicationId()
                                    << "due to private visibility";
                return false;
            } else if (!intent.requiredCapabilities().isEmpty()
                       && !m_systemInterface->checkApplicationCapabilities(requestingApplicationId,
                                                                           intent.requiredCapabilities())) {
                qCDebug(LogIntents) << "Not considering" << intent.intentId() << "/" << intent.applicationId()
                                    << "due to missing capabilities";
                return false;
            }
        }
        return true;
    });
    return result;
}

IntentList IntentServer::intentList() const
{
    return convertToQml(all());
}

Intent IntentServer::find(const QString &intentId, const QString &applicationId, const QVariantMap &parameters) const
{
    auto it = std::find_if(m_intents.cbegin(), m_intents.cend(),
                           [intentId, applicationId, parameters](const Intent &intent) -> bool {
        return (intent.applicationId() == applicationId) && (intent.intentId() == intentId)
                && intent.checkParameterMatch(parameters);
    });
    return (it != m_intents.cend()) ? *it : Intent();
}

void IntentServer::triggerRequestQueue()
{
    QMetaObject::invokeMethod(this, &IntentServer::processRequestQueue, Qt::QueuedConnection);
}

void IntentServer::enqueueRequest(IntentServerRequest *irs)
{
    qCDebug(LogIntents) << "Enqueueing Intent request:" << irs << irs->requestId() << irs->state();
    m_requestQueue.enqueue(irs);
    triggerRequestQueue();
}

void IntentServer::processRequestQueue()
{
    if (m_requestQueue.isEmpty())
        return;

    IntentServerRequest *isr = m_requestQueue.takeFirst();

    qCDebug(LogIntents) << "Processing intent request" << isr << isr->requestId() << "in state" << isr->state();

    if (isr->state() == IntentServerRequest::State::ReceivedRequest) { // step 1) disambiguate
        if (isr->handlingApplicationId().isEmpty()) {
            // not disambiguated yet

            if (!isSignalConnected(QMetaMethod::fromSignal(&IntentServer::disambiguationRequest))) {
                // If the System-UI does not react to the signal, then just use the first match.
                isr->setHandlingApplicationId(isr->potentialIntents().first().applicationId());
            } else {
                m_disambiguationQueue.enqueue(isr);
                isr->setState(IntentServerRequest::State::WaitingForDisambiguation);
                emit disambiguationRequest(isr->requestId(), convertToQml(isr->potentialIntents()),
                                           isr->parameters());
            }
        }
        if (!isr->handlingApplicationId().isEmpty()) {
            qCDebug(LogIntents) << "No disambiguation necessary/required for intent" << isr->intentId();
            isr->setState(IntentServerRequest::State::Disambiguated);
        }
    }

    if (isr->state() == IntentServerRequest::State::Disambiguated) { // step 2) start app
        auto handlerIPC = m_systemInterface->findClientIpc(isr->handlingApplicationId());
        if (!handlerIPC) {
            qCDebug(LogIntents) << "Intent handler" << isr->handlingApplicationId() << "is not running";
            m_startingAppQueue.enqueue(isr);
            isr->setState(IntentServerRequest::State::WaitingForApplicationStart);
            m_systemInterface->startApplication(isr->handlingApplicationId());
        } else {
            qCDebug(LogIntents) << "Intent handler" << isr->handlingApplicationId() << "is already running";
            isr->setState(IntentServerRequest::State::StartedApplication);
        }
    }

    if (isr->state() == IntentServerRequest::State::StartedApplication) { // step 3) send request out
        auto clientIPC = m_systemInterface->findClientIpc(isr->handlingApplicationId());
        if (!clientIPC) {
            qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                  << isr->handlingApplicationId() << "to forward the intent request"
                                  << isr->requestId();
            isr->setRequestFailed(qSL("No IPC channel to reach target application."));
        } else {
            qCDebug(LogIntents) << "Sending intent request to handler application"
                                << isr->handlingApplicationId();
            m_sentToAppQueue.enqueue(isr);
            m_systemInterface->requestToApplication(clientIPC, isr);
            isr->setState(IntentServerRequest::State::WaitingForReplyFromApplication);
        }
    }

    if (isr->state() == IntentServerRequest::State::ReceivedReplyFromApplication) { // step 5) send reply to requesting app
        auto clientIPC = m_systemInterface->findClientIpc(isr->requestingApplicationId());
        if (!clientIPC) {
            qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                  << isr->requestingApplicationId() << "to forward the Intent reply"
                                  << isr->requestId();
        } else {
            qCDebug(LogIntents) << "Forwarding intent reply" << isr->requestId()
                                << "to requesting application" << isr->requestingApplicationId();
            m_systemInterface->replyFromSystem(clientIPC, isr);
        }
        QMetaObject::invokeMethod(this, [isr]() { delete isr; }, Qt::QueuedConnection); // aka deleteLater for non-QObject
        isr = nullptr;
    }

    triggerRequestQueue();
}

IntentList IntentServer::convertToQml(const QVector<Intent> &intents)
{
    QVariantList vl;
    for (auto intent : intents)
        vl << QVariant::fromValue(intent);
    return vl;
}

void IntentServer::acknowledgeDisambiguationRequest(const QUuid &requestId, const Intent &selectedIntent)
{
    internalDisambiguateRequest(requestId, false, selectedIntent);
}

void IntentServer::rejectDisambiguationRequest(const QUuid &requestId)
{
    internalDisambiguateRequest(requestId, true, Intent());
}

void IntentServer::internalDisambiguateRequest(const QUuid &requestId, bool reject, const Intent &selectedIntent)
{
    IntentServerRequest *irs = nullptr;
    for (int i = 0; i < m_disambiguationQueue.size(); ++i) {
        if (m_disambiguationQueue.at(i)->requestId() == requestId) {
            irs = m_disambiguationQueue.takeAt(i);
            break;
        }
    }

    if (!irs) {
        qmlWarning(this) << "Got a disambiguation acknowledge or reject for intent" << requestId
                         << "but no disambiguation was expected for this intent";
    } else {
        if (reject) {
            irs->setRequestFailed(qSL("Disambiguation was rejected"));
        } else if (irs->potentialIntents().contains(selectedIntent)) {
            irs->setHandlingApplicationId(selectedIntent.applicationId());
            irs->setState(IntentServerRequest::State::Disambiguated);
        } else {
            qCWarning(LogIntents) << "IntentServer::acknowledgeDisambiguationRequest for intent"
                                  << requestId << "tried to disambiguate to the intent" << selectedIntent.intentId()
                                  << "which was not in the list of potential disambiguations";

            irs->setRequestFailed(qSL("Failed to disambiguate"));
        }
        enqueueRequest(irs);
    }
}

void IntentServer::applicationWasStarted(const QString &applicationId)
{
    // check if any intent request is waiting for this app to start
    bool foundOne = false;
    for (int i = 0; i < m_startingAppQueue.size(); ++i) {
        auto irs = m_startingAppQueue.at(i);
        if (irs->handlingApplicationId() == applicationId) {
            qCDebug(LogIntents) << "Intent request" << irs->intentId()
                                << "can now be forwarded to application" << applicationId;

            irs->setState(IntentServerRequest::State::StartedApplication);
            m_requestQueue << m_startingAppQueue.takeAt(i);
            foundOne = true;
        }
    }
    if (foundOne)
        triggerRequestQueue();
}

void IntentServer::replyFromApplication(const QString &replyingApplicationId, const QUuid &requestId,
                                        bool error, const QVariantMap &result)
{
    IntentServerRequest *irs = nullptr;
    for (int i = 0; i < m_sentToAppQueue.size(); ++i) {
        if (m_sentToAppQueue.at(i)->requestId() == requestId) {
            irs = m_sentToAppQueue.takeAt(i);
            break;
        }
    }

    if (!irs) {
        qCWarning(LogIntents) << "Got a reply for intent" << requestId << "from application"
                              << replyingApplicationId << "but no reply was expected for this intent";
    } else {
        if (irs->handlingApplicationId() != replyingApplicationId) {
            qCWarning(LogIntents) << "Got a reply for intent" << irs->requestId() << "from application"
                                  << replyingApplicationId << "but expected a reply from"
                                  << irs->handlingApplicationId() << "instead";
            irs->setRequestFailed(qSL("Request reply received from wrong application"));
        } else {
            QString errorMessage;
            if (error) {
                errorMessage = result.value(qSL("errorMessage")).toString();
                qCDebug(LogIntents) << "Got an error reply for intent" << irs->requestId() << "from application"
                                    << replyingApplicationId << ":" << errorMessage;
                irs->setRequestFailed(errorMessage);
            } else {
                qCDebug(LogIntents) << "Got a reply for intent" << irs->requestId() << "from application"
                                    << replyingApplicationId << ":" << result;
                irs->setRequestSucceeded(result);
            }
        }
        enqueueRequest(irs);
    }
}

IntentServerRequest *IntentServer::requestToSystem(const QString &requestingApplicationId,
                                                   const QString &intentId, const QString &applicationId,
                                                   const QVariantMap &parameters)
{
    qCDebug(LogIntents) << "Server: Incoming intent request" << intentId << "from application"
                        << requestingApplicationId << "to application" << applicationId;

    if (!m_systemInterface->findClientIpc(requestingApplicationId)) {
        qCWarning(LogIntents) << "Intent" << intentId << "was requested from unknown application"
                              << requestingApplicationId;
        return nullptr;
    }

    QVector<Intent> intents;
    if (applicationId.isEmpty())
        intents = filterByIntentId(all(), intentId, parameters);
    else if (Intent intent = find(intentId, applicationId, parameters))
        intents << intent;

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Unknown intent" << intentId << "was requested from application"
                              << requestingApplicationId;
        return nullptr;
    }

    intents = filterByRequestingApplicationId(intents, requestingApplicationId);

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Inaccessible intent" << intentId << "was requested from application"
                              << requestingApplicationId;
        return nullptr;
    }

    auto irs = new IntentServerRequest(requestingApplicationId, intentId, intents, parameters);
    enqueueRequest(irs);
    return irs;
}

QT_END_NAMESPACE_AM
