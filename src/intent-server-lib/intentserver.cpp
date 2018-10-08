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
#include <QTimer>
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

    qmlRegisterSingletonType<IntentServer>("QtApplicationManager", 1, 0, "IntentManager",
                                           &IntentServer::instanceForQml);
    return s_instance = is.take();
}

IntentServer *IntentServer::instance()
{
    if (!s_instance)
        qFatal("IntentServer::instance() was called before createInstance().");
    return s_instance;
}

QObject *IntentServer::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}


IntentServer::IntentServer(IntentServerSystemInterface *systemInterface, QObject *parent)
    : QObject(parent)
    , m_systemInterface(systemInterface)
{
    m_systemInterface->setParent(this);
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

const Intent *IntentServer::addIntent(const QString &id, const QString &applicationId, const QStringList &capabilities, Intent::Visibility visibility, const QVariantMap &parameterMatch)
{
    return addIntent(id, applicationId, QString(), capabilities, visibility, parameterMatch);
}

const Intent *IntentServer::addIntent(const QString &id, const QString &applicationId, const QString &backgroundHandlerId, const QStringList &capabilities, Intent::Visibility visibility, const QVariantMap &parameterMatch)
{
    if (id.isEmpty()
            || !m_knownApplications.contains(applicationId)
            || find(id, applicationId)
            || (!backgroundHandlerId.isEmpty()
                && !m_knownBackgroundServices[applicationId].contains(backgroundHandlerId))) {
        return nullptr;
    }

    auto intent = new Intent(id, applicationId, backgroundHandlerId, capabilities, visibility,
                             parameterMatch);
    m_intents << intent;
    emit intentAdded(intent);
    return intent;
}

void IntentServer::removeIntent(const Intent *intent)
{
    if (intent) {
        int pos = m_intents.indexOf(intent);

        if (pos >= 0) {
            m_intents.removeAt(pos);
            emit intentRemoved(intent);
        }
    }
}

QVector<const Intent *> IntentServer::all() const
{
    return m_intents;
}

QVector<const Intent *> IntentServer::filterByIntentId(const QString &intentId, const QVariantMap &parameters) const
{
    QVector<const Intent *> result;
    std::copy_if(m_intents.cbegin(), m_intents.cend(), std::back_inserter(result),
                 [intentId, parameters](const Intent *intent) -> bool {
        return (intent->id() == intentId) && intent->checkParameterMatch(parameters);

    });
    return result;
}

QVector<const Intent *> IntentServer::filterByApplicationId(const QString &applicationId, const QVariantMap &parameters) const
{
    QVector<const Intent *> result;
    std::copy_if(m_intents.cbegin(), m_intents.cend(), std::back_inserter(result),
                 [applicationId, parameters](const Intent *intent) -> bool {
        return (intent->applicationId() == applicationId) && intent->checkParameterMatch(parameters);

    });
    return result;
}

const Intent *IntentServer::find(const QString &intentId, const QString &applicationId, const QVariantMap &parameters) const
{
    auto it = std::find_if(m_intents.cbegin(), m_intents.cend(),
                           [intentId, applicationId, parameters](const Intent *intent) -> bool {
        return (intent->applicationId() == applicationId) && (intent->id() == intentId)
                && intent->checkParameterMatch(parameters);
    });
    return (it != m_intents.cend()) ? *it : nullptr;
}


void IntentServer::triggerRequestQueue()
{
    QTimer::singleShot(0, this, &IntentServer::processRequestQueue);
}

void IntentServer::enqueueRequest(IntentServerRequest *irs)
{
    qCDebug(LogIntents) << "Enqueueing Intent request:" << irs << irs->id() << irs->state() << m_requestQueue.size();
    m_requestQueue.enqueue(irs);
    triggerRequestQueue();
}

void IntentServer::processRequestQueue()
{
    if (m_requestQueue.isEmpty())
        return;

    IntentServerRequest *irs = m_requestQueue.takeFirst();

    qCDebug(LogIntents) << "Processing intent request" << irs << irs->id() << "in state" << irs->state()  << m_requestQueue.size();

    if (irs->state() == IntentServerRequest::State::ReceivedRequest) { // step 1) disambiguate
        if (!irs->m_actualIntent) {
            // not disambiguated yet

            if (!isSignalConnected(QMetaMethod::fromSignal(&IntentServer::disambiguationRequest))) {
                // If the System-UI does not react to the signal, then just use the first match.
                irs->m_actualIntent = irs->m_intents.first();
            } else {
                m_disambiguationQueue.enqueue(irs);
                irs->setState(IntentServerRequest::State::WaitingForDisambiguation);
                emit disambiguationRequest(irs->id(), irs->m_intents.first()->id(), irs->m_intents, irs->m_parameters);
            }
        }
        if (irs->intent()) {
            qCDebug(LogIntents) << "No disambiguation necessary/required for intent" << irs->intent()->id();
            irs->setState(IntentServerRequest::State::Disambiguated);
        }
    }

    if (irs->state() == IntentServerRequest::State::Disambiguated) { // step 2) start app
        auto handlerIPC = m_systemInterface->findClientIpc(irs->intent()->applicationId());
        if (!handlerIPC) {
            qCDebug(LogIntents) << "Intent target app" << irs->intent()->applicationId() << "is not running";
            m_startingAppQueue.enqueue(irs);
            irs->setState(IntentServerRequest::State::WaitingForApplicationStart);
            m_systemInterface->startApplication(irs->intent()->applicationId());
        } else {
            qCDebug(LogIntents) << "Intent target app" << irs->intent()->applicationId() << "is already running";
            irs->setState(IntentServerRequest::State::StartedApplication);
        }
    }

    if (irs->state() == IntentServerRequest::State::StartedApplication) { // step 3) send request out
        auto clientIPC = m_systemInterface->findClientIpc(irs->intent()->applicationId());
        if (!clientIPC) {
            qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                 << irs->intent()->applicationId() << "to forward the intent request"
                                 << irs->id();
            irs->requestFailed(qSL("No IPC channel to reach target application."));
        } else {
            qCDebug(LogIntents) << "Sending intent request to application" << irs->intent()->applicationId();
            m_sentToAppQueue.enqueue(irs);
            m_systemInterface->requestToApplication(clientIPC, irs);
            irs->setState(IntentServerRequest::State::WaitingForReplyFromApplication);
        }
    }

    if (irs->state() == IntentServerRequest::State::ReceivedReplyFromApplication) { // step 5) send reply to requesting app
        auto clientIPC = m_systemInterface->findClientIpc(irs->m_requestingAppId);
        if (!clientIPC) {
            qCWarning(LogIntents) << "Could not find an IPC connection for application"
                                 << irs->m_requestingAppId << "to forward the Intent reply"
                                 << irs->id();
        } else {
            qCDebug(LogIntents) << "Forwarding intent reply" << irs->id() << "to requesting application"
                               << irs->m_requestingAppId;
            m_systemInterface->replyFromSystem(clientIPC, irs);
        }
        QTimer::singleShot(0, this, [irs]() { delete irs; }); // aka deleteLater for non-QObject
        irs = nullptr;
    }

    triggerRequestQueue();
}

void IntentServer::acknowledgeDisambiguationRequest(const QUuid &requestId, const Intent *intent)
{
    IntentServerRequest *irs = nullptr;
    for (int i = 0; i < m_disambiguationQueue.size(); ++i) {
        if (m_disambiguationQueue.at(i)->id() == requestId) {
            irs = m_disambiguationQueue.takeAt(i);
            break;
        }
    }

    if (!irs) {
        qmlWarning(this) << "Got a disambiguation acknowledge for intent" << requestId
                         << "but no disambiguation was expected for this intent";
    } else {
        if (irs->m_intents.contains(intent)) {
            irs->m_actualIntent = intent;
            irs->setState(IntentServerRequest::State::Disambiguated);
        } else {
            qCWarning(LogIntents) << "IntentServer::acknowledgeDisambiguationRequest for intent"
                                 << requestId << "tried to disambiguate to the intent"
                                 << (intent ? intent->id() : qSL("<null>"))
                                 << "which was not in the list of available disambiguations";

            irs->requestFailed(qSL("Failed to disambiguate"));
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
        if (irs->intent()->applicationId() == applicationId) {
            qCDebug(LogIntents) << "Intent request" << irs->intent()->id()
                               << "can now be forwarded to application" << applicationId;

            irs->setState(IntentServerRequest::State::StartedApplication);
            m_requestQueue << m_startingAppQueue.takeAt(i);
            foundOne = true;
        }
    }
    if (foundOne)
        triggerRequestQueue();
}

void IntentServer::replyFromApplication(const QString &replyingApplicationId, const QString &requestId, bool error, const QVariantMap &result)
{
    IntentServerRequest *irs = nullptr;
    for (int i = 0; i < m_sentToAppQueue.size(); ++i) {
        if (m_sentToAppQueue.at(i)->id() == requestId) {
            irs = m_sentToAppQueue.takeAt(i);
            break;
        }
    }

    if (!irs) {
        qCWarning(LogIntents) << "Got a reply for intent" << requestId << "from application"
                             << replyingApplicationId << "but no reply was expected for this intent";
    } else {
        if (irs->intent()->applicationId() != replyingApplicationId) {
            qCWarning(LogIntents) << "Got a reply for intent" << irs->id() << "from application"
                                 << replyingApplicationId << "but expected a reply from"
                                 << irs->intent()->applicationId() << "instead";
            irs->requestFailed(qSL("Request reply received from wrong application"));
        } else {
            QString errorMessage;
            if (error) {
                errorMessage = result.value(qSL("errorMessage")).toString();
                qCDebug(LogIntents) << "Got an error reply for intent" << irs->id() << "from application"
                                   << replyingApplicationId << ":" << errorMessage;
                irs->requestFailed(errorMessage);
            } else {
                qCDebug(LogIntents) << "Got a reply for intent" << irs->id() << "from application"
                                   << replyingApplicationId << ":" << result;
                irs->requestSucceeded(result);
            }
        }
        enqueueRequest(irs);
    }
}

IntentServerRequest *IntentServer::requestToSystem(const QString &requestingApplicationId, const QString &intentId, const QString &applicationId, const QVariantMap &parameters)
{
    qCDebug(LogIntents) << "Incoming intent request" << intentId << "from application"
                       << requestingApplicationId << "to application" << applicationId;

    QVector<const Intent *> intents;
    if (applicationId.isEmpty())
        intents = filterByIntentId(intentId, parameters);
    else if (const Intent *intent = find(intentId, applicationId, parameters))
        intents << intent;

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Unknown intent" << intentId << "was requested from application"
                             << requestingApplicationId;
        return nullptr;
    }

    // filter on visibility and capabilities
    //TODO: move this part to a separate filter() function?
    for (auto it = intents.begin(); it != intents.end(); ) {
        const Intent *intent = *it;
        bool keep = true;

        if ((intent->visibility() == Intent::Private)
                && (intent->applicationId() != requestingApplicationId)) {
            qCDebug(LogIntents) << "Not considering" << intent->id() << "/" << intent->applicationId()
                               << "due to private visibility";
            keep = false;
        }
        else if (!intent->requiredCapabilities().isEmpty()
                && !m_systemInterface->checkApplicationCapabilities(requestingApplicationId,
                                                                    intent->requiredCapabilities())) {
            qCDebug(LogIntents) << "Not considering" << intent->id() << "/" << intent->applicationId()
                               << "due to missing capabilities";
            keep = false;
        }
        if (!keep)
            intents.erase(it);
        else
            ++it;
    }

    if (intents.isEmpty()) {
        qCWarning(LogIntents) << "Inaccessible intent" << intentId << "was requested from application"
                             << requestingApplicationId;
        return nullptr;
    }

    auto irs = new IntentServerRequest(true, requestingApplicationId, intents, parameters);
    enqueueRequest(irs);
    return irs;
}

QT_END_NAMESPACE_AM
