/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include <QQmlInfo>

#include "intenthandler.h"
#include "intentclient.h"

QT_BEGIN_NAMESPACE_AM

/*! \qmltype IntentHandler
    \inqmlmodule QtApplicationManager.Application
    \ingroup application-instantiatable
    \brief A handler for intent requests received by applications.

    Any application that has intents listed in its manifest file needs to have a corresponding
    IntentHandler instance that is actually able to handle incoming requests. This class gives
    you the flexibility to handle multiple, different intent ids via a single IntentHandler
    instance or have a dedicated IntentHandler instance for every intent id (or any combination of
    those).

    \note For handling intent requests within the System UI, you have to use the derived component
          IntentServerHandler, which works the same way, but provides all the necessary meta-data
          from within QML.

    Here is a fairly standard way to handle an incoming intent request and send out a result or
    error message:

    \qml
    Image {
        id: viewer
    }

    IntentHandler {
        intentIds: [ "show-image" ]
        onRequestReceived: {
            var url = request.parameters["url"]
            if (!url.startsWith("file://")) {
                request.sendErrorReply("Only file:// urls are supported")
            } else {
                viewer.source = url
                request.sendReply({ "status": source.status })
            }
        }
    }
    \endqml

*/

/*! \qmlproperty list<string> IntentHandler::intentIds

    Every handler needs to register at least one unique intent id that it will handle. Having
    multiple IntentHandlers that are registering the same intent id is not possible.

    \note Any changes to this property after component completion will have no effect.
*/

/*! \qmlsignal IntentHandler::requestReceived(IntentRequest request)

    This signal will be emitted once for every incoming intent \a request that this handler was
    registered for via its intentIds property.

    Handling the request can be done synchronously or asynchronously.  As soon as your handler has
    either produced a result or detected an error condition, it should call
    IntentRequest::sendReply() or IntentRequest::sendErrorReply() respectively to send a reply back
    to the requesting party.  Even if your intent does not have a return value, you still need to
    send an empty object \c{{}} reply to signal that the intent request has been handled
    successfully.

    Only the first call to one of these functions will have any effect.  Any further invocations
    will be ignored.

    If these functions are not called after receiving an intent request within the \l{Intent
    Timeout Specification}{system's specified timeout interval}, the system will send an implicit
    "failed due to timeout" error reply back to the original sender.
*/

IntentHandler::IntentHandler(QObject *parent)
    : QObject(parent)
{ }

IntentHandler::~IntentHandler()
{
    if (auto ie = IntentClient::instance())
        ie->unregisterHandler(this);
}

QStringList IntentHandler::intentIds() const
{
    return m_intentIds;
}

void IntentHandler::setIntentIds(const QStringList &intentIds)
{
    if (isComponentCompleted()) {
        qmlWarning(this) << "Cannot change the intentIds property of an IntentHandler after creation.";
        return;
    }
    if (m_intentIds != intentIds) {
        m_intentIds = intentIds;
        emit intentIdsChanged(intentIds);
    }
}

void IntentHandler::componentComplete()
{
    IntentClient::instance()->registerHandler(this);
    m_completed = true;
}

void IntentHandler::classBegin()
{ }

bool IntentHandler::isComponentCompleted() const
{
    return m_completed;
}

QT_END_NAMESPACE_AM

#include "moc_intenthandler.cpp"
