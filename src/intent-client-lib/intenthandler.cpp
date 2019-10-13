/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include "intenthandler.h"
#include "intentclient.h"

QT_BEGIN_NAMESPACE_AM

/*! \qmltype IntentHandler
    \inqmlmodule QtApplicationManager.Application
    \ingroup common-instantiatable
    \brief A handler for intent requests received by applications.

    Any application that has intents listed in its manifest file needs to have a corresponding
    IntentHandler instance that is actually able to handle incoming requests. This class gives
    you the flexibility to handle multiple, different intent ids via a single IntentHandler
    instance or have a dedicated IntentHandler instance for every intent id (or any combination of
    those).

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

    \note Any changes to this property after component completion will have no effect. This
          restriction will likely be removed in a future update.
*/

/*! \qmlsignal IntentHandler::requestReceived(IntentRequest request)

    This signal will be emitted once for every incoming intent \a request that this handler was
    registered for via its intentIds property.
    Handling the request can be done synchronously or asynchronously. As soon as your handler has
    either produced a result or detected an error condition, it should call either
    IntentRequest::sendReply() or IntentRequest::sendErrorReply respectively to send a
    reply back to the requesting party.
    Only the first call to one of these functions will have any effect. Any further invocations
    will be ignored.
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
    if (intentIds != m_intentIds) {
        m_intentIds = intentIds;
        emit intentIdsChanged(m_intentIds);
    }
}

void IntentHandler::componentComplete()
{
    IntentClient::instance()->registerHandler(this);
}

void IntentHandler::classBegin()
{ }

QT_END_NAMESPACE_AM
