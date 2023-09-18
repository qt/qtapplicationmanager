/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QScopedPointer>
#include <QQmlEngine>
#include <QQmlInfo>

#include "intentclient.h"
#include "intentclientsysteminterface.h"
#include "intentclientrequest.h"
#include "intenthandler.h"
#include "logging.h"

#include <exception>

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype IntentClient
    \inqmlmodule QtApplicationManager
    \ingroup common-singletons
    \brief Singleton that provides functions to create Intent requests.

    This type can be used both in applications as well as within the System UI to create intent
    requests. This type is only the factory, returning instances of the type IntentRequest. See
    the IntentRequest documentation for details on how to actually handle these asynchronous calls.

    Here is a fairly standard way to send an intent request and react on its result (or error
    message):

    \qml
    MouseArea {
        onClicked: {
            var request = IntentClient.sendIntentRequest("show-image", { url: "file://x.png" })
            request.onReplyReceived.connect(function() {
                if (request.succeeded)
                    var result = request.result
                else
                    console.log("Intent request failed: " + request.errorMessage)
            })
        }
    }
    \endqml
*/

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
    return s_instance = ic.take();
}

IntentClient *IntentClient::instance()
{
    if (!s_instance)
        qFatal("IntentClient::instance() was called before createInstance().");
    return s_instance;
}

void IntentClient::setReplyFromSystemTimeout(int timeout)
{
    m_replyFromSystemTimeout = timeout;
}

void IntentClient::setReplyFromApplicationTimeout(int timeout)
{
    m_replyFromApplicationTimeout = timeout;
}

IntentClient::IntentClient(IntentClientSystemInterface *systemInterface, QObject *parent)
    : QObject(parent)
    , m_systemInterface(systemInterface)
{
    m_lastWaitingCleanup.start();
    m_systemInterface->setParent(this);
}

IntentClient::~IntentClient()
{
    s_instance = nullptr;
}

void IntentClient::registerHandler(IntentHandler *handler)
{
    QString applicationId = m_systemInterface->currentApplicationId(handler);

    qCDebug(LogIntents) << "Client: Registering intent handler" << handler << "for" << handler->intentIds()
                        << "for application" << applicationId;

    const QStringList intentIds = handler->intentIds();
    for (auto intentId : intentIds) {
        auto key = qMakePair(intentId, applicationId);

        if (m_handlers.contains(key)) {
            qmlWarning(handler) << "Double registration for intent " << intentId << " within application "
                                << applicationId << " detected. Only the handler that registered first will be active.";
        } else {
            m_handlers.insert(key, handler);
        }
    }
}

void IntentClient::unregisterHandler(IntentHandler *handler)
{
    m_handlers.removeIf([handler](auto it) { return it.value() == handler; });
}

/*! \qmlmethod IntentRequest IntentClient::sendIntentRequest(string intentId, var parameters)

    Sends a request for an intent with the given \a intentId to the system. The additional
    \a parameters are specific to the requested \a intentId, but the format is always the same: a
    standard JavaScript object, which can also be just empty if the requested intent doesn't
    require any parameters.

    Returns an IntentRequest object that can be used to track this asynchronous request.

    \note The returned object has JavaScript ownership, which means that you do not have to worry
          about freeing resources. Even just ignoring the return value is fine, if you are not
          interested in the result (or error condition) of your request.
*/
IntentClientRequest *IntentClient::sendIntentRequest(const QString &intentId, const QVariantMap &parameters)
{
    return sendIntentRequest(intentId, QString(), parameters);
}

/*! \qmlmethod IntentRequest IntentClient::sendIntentRequest(string intentId, string applicationId, var parameters)
    \overload

    Instead of letting the System UI (or the user) choose which application should handle your
    request, you can use this overload to hardcode the \a applicationId that is required to handle
    it. The request will fail, if this specified application doesn't exist or can't handle this
    specific request, even though other applications would be able to do it.

    \sa sendIntentRequest
*/
IntentClientRequest *IntentClient::sendIntentRequest(const QString &intentId, const QString &applicationId,
                                                     const QVariantMap &parameters)
{
    if (intentId.isEmpty())
        return nullptr;

    //TODO: check that parameters only contains basic datatypes. convertFromJSVariant() does most of
    //      this already, but doesn't bail out on unconvertible types (yet)

    auto icr = requestToSystem(m_systemInterface->currentApplicationId(this), intentId, applicationId, parameters);
    QQmlEngine::setObjectOwnership(icr, QQmlEngine::JavaScriptOwnership);
    icr->startTimeout(m_replyFromSystemTimeout);
    return icr;
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
    if (!icr)
        return;

    if (error) {
        icr->setErrorMessage(errorMessage);
    } else if (newRequestId.isNull()) {
        icr->setErrorMessage(qL1S("No matching Intent found in the system"));
    } else {
        icr->setRequestId(newRequestId);
        m_waiting << icr;
    }
}

void IntentClient::replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result)
{
    IntentClientRequest *icr = nullptr;
    auto it = std::find_if(m_waiting.cbegin(), m_waiting.cend(),
                           [requestId](const QPointer<IntentClientRequest> &ir) -> bool {
            return ir && (ir->requestId() == requestId);
    });

    if (it == m_waiting.cend()) {
        qCWarning(LogIntents) << "IntentClient received an unexpected intent reply for request"
                              << requestId << " succeeded:" << !error << "error:"
                              << result.value(qL1S("errorMessage")).toString() << "result:" << result;
        return;
    }
    icr = *it;
    m_waiting.erase(it);

    // make sure to periodically remove all requests that were gc'ed before a reply was received
    if (m_lastWaitingCleanup.elapsed() > 1000) {
        m_waiting.removeAll({ });
        m_lastWaitingCleanup.start();
    }

    if (error)
        icr->setErrorMessage(result.value(qSL("errorMessage")).toString());
    else
        icr->setResult(result);

    qCDebug(LogIntents) << "Application" << icr->requestingApplicationId() << "received an intent reply for"
                        << icr->intentId() << " succeeded:" << icr->succeeded() << "error:"
                        << icr->errorMessage() << "result:" << icr->result();
}

void IntentClient::requestToApplication(const QUuid &requestId, const QString &intentId,
                                        const QString &requestingApplicationId,
                                        const QString &applicationId, const QVariantMap &parameters)
{
    qCDebug(LogIntents) << "Client: Incoming intent request" << requestId << "to application" << applicationId
                        << "for intent" << intentId << "parameters" << parameters;

    IntentClientRequest *icr = new IntentClientRequest(IntentClientRequest::Direction::ToApplication,
                                                       requestingApplicationId, requestId, intentId,
                                                       applicationId, parameters);

    IntentHandler *handler = m_handlers.value(qMakePair(intentId, applicationId));
    if (handler) {
        QQmlEngine::setObjectOwnership(icr, QQmlEngine::JavaScriptOwnership);
        icr->startTimeout(m_replyFromApplicationTimeout);

        emit handler->requestReceived(icr);
    } else {
        qCDebug(LogIntents) << "No Intent handler registered for intent" << intentId;
        errorReplyFromApplication(icr, qSL("No matching IntentHandler found."));
        delete icr;
    }
}

void IntentClient::replyFromApplication(IntentClientRequest *icr, const QVariantMap &result)
{
    if (!icr || icr->m_direction != IntentClientRequest::Direction::ToApplication)
        return;
    icr->m_succeeded = true;
    icr->m_finished = true;
    icr->m_result = result;

    m_systemInterface->replyFromApplication(icr);
}

void IntentClient::errorReplyFromApplication(IntentClientRequest *icr, const QString &errorMessage)
{
    if (!icr || icr->m_direction != IntentClientRequest::Direction::ToApplication)
        return;
    icr->m_succeeded = false;
    icr->m_finished = true;
    icr->m_result = QVariantMap{ { qSL("errorMessage"), errorMessage } };

    m_systemInterface->replyFromApplication(icr);
}

QT_END_NAMESPACE_AM

#include "moc_intentclient.cpp"
