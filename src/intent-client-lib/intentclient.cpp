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

#include <QScopedPointer>
#include <qqml.h>
#include <QQmlInfo>

#include "intentclient.h"
#include "intentclientsysteminterface.h"
#include "intentclientrequest.h"
#include "intenthandler.h"
#include "logging.h"

#include <exception>

QT_BEGIN_NAMESPACE_AM

IntentClient *IntentClient::s_instance = nullptr;

IntentClient *IntentClient::createInstance(IntentClientSystemInterface *systemInterface)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("IntentClient::createInstance() was called a second time.");
    if (Q_UNLIKELY(!systemInterface))
        qFatal("IntentClient::createInstance() was called without a systemInterface.");

    QScopedPointer<IntentClient> ic(new IntentClient(systemInterface));
    try {
        systemInterface->initialize(ic.data());
    } catch (const std::exception &exc) {
        qCWarning(LogIntents) << "Failed to initialize IntentClient:" << exc.what();
        return nullptr;
    }

    qmlRegisterUncreatableType<IntentClientRequest>("QtApplicationManager", 1, 0, "IntentRequest",
                                                    qSL("Cannot create objects of type IntentRequest"));
    qmlRegisterType<IntentHandler>("QtApplicationManager", 1, 0, "IntentHandler");

    return s_instance = ic.take();
}

IntentClient *IntentClient::instance()
{
    if (!s_instance)
        qFatal("IntentClient::instance() was called before createInstance().");
    return s_instance;
}

IntentClient::IntentClient(IntentClientSystemInterface *systemInterface, QObject *parent)
    : QObject(parent)
    , m_systemInterface(systemInterface)
{
    m_systemInterface->setParent(this);
}

IntentClient::~IntentClient()
{
    s_instance = nullptr;
}

void IntentClient::registerHandler(IntentHandler *handler)
{
    const QStringList intentIds = handler->intentIds();
    for (auto intentId : intentIds) {
        if (m_handlers.contains(intentId)) {
            qmlWarning(handler) << "Double registration for intent" << intentId << "detected. "
                                                                                   "Only the handler that registered first will be active.";
        } else {
            m_handlers.insert(intentId, handler);
        }
    }
}

void IntentClient::unregisterHandler(IntentHandler *handler)
{
    const QStringList intentIds = handler->intentIds();
    for (auto intentId : intentIds)
        m_handlers.remove(intentId);
}

IntentClientRequest *IntentClient::requestToSystem(const QString &requestingApplicationId,
                                                   const QString &intentId, const QString &applicationId,
                                                   const QVariantMap &parameters)
{
    IntentClientRequest *ir = new IntentClientRequest(IntentClientRequest::Direction::ToSystem,
                                                      requestingApplicationId, QUuid(),
                                                      intentId, applicationId, parameters);

    qCDebug(LogIntents) << "Application" << requestingApplicationId << "created an intent request for"
                        << intentId << "(application:" << applicationId << ")";
    m_systemInterface->requestToSystem(ir);
    return ir;
}

void IntentClient::requestToSystemFinished(IntentClientRequest *icr, const QUuid &newRequestId, bool error, const QString &errorMessage)
{
    if (error) {
        icr->setErrorMessage(errorMessage);
        icr->deleteLater();
    } else if (newRequestId.isNull()) {
        icr->setErrorMessage(qL1S("No matching Intent found in the system"));
        icr->deleteLater();
    } else {
        icr->setRequestId(newRequestId);
        m_waiting << icr;
    }
}

void IntentClient::replyFromSystem(const QString &requestId, bool error, const QVariantMap &result)
{
    IntentClientRequest *icr = nullptr;
    auto it = std::find_if(m_waiting.begin(), m_waiting.end(),
                           [requestId](IntentClientRequest *ir) -> bool {
            return (ir->id() == requestId);
});
    if (it == m_waiting.cend()) {
        qCWarning(LogIntents) << "IntentClient received an unexpected intent reply for request"
                              << requestId << " succeeded:" << error << "error:"
                              << result.value(qL1S("errorMessage")).toString() << "result:" << result;
        return;
    }
    icr = *it;
    m_waiting.erase(it);

    if (error)
        icr->setErrorMessage(result.value(qSL("errorMessage")).toString());
    else
        icr->setResult(result);
    icr->deleteLater();

    qCDebug(LogIntents) << "Application" << icr->requestingApplicationId() << "received an intent reply for"
                        << icr->intentId() << " succeeded:" << icr->succeeded() << "error:"
                        << icr->errorMessage() << "result:" << icr->result();
}

void IntentClient::requestToApplication(const QString &requestId, const QString &intentId,
                                        const QString &applicationId, const QVariantMap &parameters)
{
    //TODO: we should sanity check the applicationId
    qCDebug(LogIntents) << "Incoming intent request" << requestId << " to application" << applicationId
                        << "for intent" << intentId << "parameters" << parameters;

    IntentClientRequest *icr = new IntentClientRequest(IntentClientRequest::Direction::ToApplication,
                                                       QString(), requestId, intentId, applicationId,
                                                       parameters);

    IntentHandler *handler = m_handlers.value(intentId);
    if (handler) {
        emit handler->receivedRequest(icr);
    } else {
        qCDebug(LogIntents) << "No Intent handler registered for intent" << intentId;
        errorReplyFromApplication(icr, qSL("No matching IntentHandler found."));
    }
}

void IntentClient::replyFromApplication(IntentClientRequest *icr, const QVariantMap &result)
{
    if (!icr || icr->m_direction != IntentClientRequest::Direction::ToApplication)
        return;
    icr->m_succeeded = true;
    icr->m_result = result;

    m_systemInterface->replyFromApplication(icr);
    icr->deleteLater();
}

void IntentClient::errorReplyFromApplication(IntentClientRequest *icr, const QString &errorMessage)
{
    if (!icr || icr->m_direction != IntentClientRequest::Direction::ToApplication)
        return;
    icr->m_succeeded = false;
    icr->m_result = QVariantMap{ { qSL("errorMessage"), errorMessage } };

    m_systemInterface->replyFromApplication(icr);
    icr->deleteLater();
}

QT_END_NAMESPACE_AM

