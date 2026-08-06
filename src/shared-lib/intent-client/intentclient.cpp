// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlInfo>

#include "intentclient.h"
#include "intentclientsysteminterface.h"
#include "intentclientrequest.h"
#include "intenthandler.h"
#include "logging.h"

#include <exception>
#include <memory>

using namespace Qt::StringLiterals;


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

    std::unique_ptr<IntentClient> ic(new IntentClient(systemInterface));
    try {
        systemInterface->initialize(ic.get());
    } catch (const std::exception &exc) {
        qCWarning(LogIntentClient) << "Failed to initialize IntentClient:" << exc.what();
        return nullptr;
    }
    return s_instance = ic.release();
}

IntentClient *IntentClient::instance()
{
    if (!s_instance)
        qFatal("IntentClient::instance() was called before createInstance().");
    return s_instance;
}

/*! \qmlproperty string IntentClient::systemUiId

    The hardcoded, special application id for targeting the System UI with an intent request.
*/
QString IntentClient::systemUiId() const
{
    return u":sysui:"_s;
}

int IntentClient::replyFromSystemTimeout() const
{
    return m_replyFromSystemTimeout;
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
    m_systemInterface->setParent(this);
}

IntentClient::~IntentClient()
{
    s_instance = nullptr;
}

void IntentClient::registerHandler(AbstractIntentHandler *handler)
{
    QString applicationId = m_systemInterface->currentApplicationId(handler);

    const QStringList intentIds = handler->intentIds();
    for (auto intentId : intentIds) {
        auto key = std::make_pair(intentId, applicationId);

        if (m_handlers.contains(key)) {
            qmlWarning(handler) << "Double registration for intent " << intentId << " within application "
                                << applicationId << " detected. Only the handler that registered first will be active.";
        } else {
            m_handlers.insert(key, handler);
        }
    }
}

void IntentClient::unregisterHandler(AbstractIntentHandler *handler)
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

    There is the special application id \c IntentClient.systemUiId which can be used to target the
    System UI.

    \sa sendIntentRequest
*/
IntentClientRequest *IntentClient::sendIntentRequest(const QString &intentId, const QString &applicationId,
                                                     const QVariantMap &parameters)
{
    if (intentId.isEmpty())
        return nullptr;
    if (applicationId == u":broadcast:") // reserved
        return nullptr;

    //TODO: check that parameters only contains basic datatypes.

    auto icr = requestToSystem(m_systemInterface->currentApplicationId(this), intentId, applicationId, parameters);
    QQmlEngine::setObjectOwnership(icr, QQmlEngine::JavaScriptOwnership);
    icr->startTimeout(m_replyFromSystemTimeout);
    return icr;
}

/*! \qmlmethod bool IntentClient::broadcastIntentRequest(string intentId, var parameters)
    \since 6.5

    Broadcasts an intent request with the given \a intentId to the system. The additional
    \a parameters are specific to the requested \a intentId, but the format is always the same: a
    standard JavaScript object, which can also be just empty if the requested intent doesn't
    require any parameters.

    Broadcast requests do not generate replies. The return value is only ever \c false, if you
    call this function with invalid arguments.
*/
bool IntentClient::broadcastIntentRequest(const QString &intentId, const QVariantMap &parameters)
{
    if (intentId.isEmpty())
        return false;

    //TODO: check that parameters only contains basic datatypes.

    requestToSystem(m_systemInterface->currentApplicationId(this), intentId, u":broadcast:"_s, parameters);
    return true;
}

bool IntentClient::isSystemUI() const
{
    return m_systemInterface->isSystemUI();
}

IntentClientRequest *IntentClient::requestToSystem(const QString &requestingApplicationId,
                                                   const QString &intentId, const QString &applicationId,
                                                   const QVariantMap &parameters)
{
    auto *ir = new IntentClientRequest(IntentClientRequest::Direction::ToSystem,
                                       requestingApplicationId, QUuid(),
                                       intentId, applicationId, parameters,
                                       applicationId == u":broadcast:");
    m_systemInterface->requestToSystem(ir);
    return ir;
}

void IntentClient::requestToSystemFinished(IntentClientRequest *icr, const QUuid &newRequestId, bool error, const QString &errorMessage)
{
    if (!icr)
        return;

    if (icr->isBroadcast()) {
        icr->deleteLater();
        return;
    }

    if (error) {
        icr->setErrorMessage(errorMessage);
    } else if (newRequestId.isNull()) {
        icr->setErrorMessage(u"No matching Intent found in the system"_s);
    } else {
        icr->setRequestId(newRequestId);
        m_waiting.insert(newRequestId, icr);
        // removes requests that got gc'ed on the JS side before a reply was received
        connect(icr, &QObject::destroyed, this, [this, newRequestId]() {
            m_waiting.remove(newRequestId);
        });
    }
}

void IntentClient::replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result)
{
    IntentClientRequest *icr = m_waiting.take(requestId);
    if (!icr) {
        qCWarning(LogIntentClient).nospace().noquote()
            << requestId.toString(QUuid::WithoutBraces)
            << " [?] {" << m_systemInterface->currentApplicationId(this) << " -> ?} "
            << "received a reply for an unknown request id";
        return;
    }

    if (error)
        icr->setErrorMessage(result.value(u"errorMessage"_s).toString());
    else
        icr->setResult(result);
}

void IntentClient::requestToApplication(const QUuid &requestId, const QString &intentId,
                                        const QString &requestingApplicationId,
                                        const QString &applicationId, const QVariantMap &parameters)
{
    bool broadcast = (requestingApplicationId == u":broadcast:");

    auto *icr = new IntentClientRequest(IntentClientRequest::Direction::ToApplication,
                                        requestingApplicationId, requestId, intentId,
                                        applicationId, parameters, broadcast);

    AbstractIntentHandler *handler = m_handlers.value(std::make_pair(intentId, applicationId));
    if (handler) {
        QQmlEngine::setObjectOwnership(icr, QQmlEngine::JavaScriptOwnership);
        if (!broadcast)
            icr->startTimeout(m_replyFromApplicationTimeout);

        handler->internalRequestReceived(icr);
    } else {
        qCDebug(LogIntentClient).nospace().noquote()
            << requestId.toString(QUuid::WithoutBraces)
            << " [" << intentId << "] {" << requestingApplicationId << " -> " << applicationId << "} "
            << "no intent handler registered";
        errorReplyFromApplication(icr, u"No matching IntentHandler found."_s);
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
    icr->m_result = QVariantMap{ { u"errorMessage"_s, errorMessage } };

    m_systemInterface->replyFromApplication(icr);
}

QT_END_NAMESPACE_AM

#include "moc_intentclient.cpp"
