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

#include "intentclientrequest.h"
#include "intentclient.h"

#include <QQmlEngine>
#include <QThread>
#include <QPointer>
#include <QTimer>

QT_BEGIN_NAMESPACE_AM


IntentClientRequest::Direction IntentClientRequest::direction() const
{
    return m_direction;
}

IntentClientRequest::~IntentClientRequest()
{
    // the incoming request was gc'ed on the JavaScript side, but no reply was sent yet
    if ((direction() == Direction::ToApplication) && !m_finished)
        sendErrorReply(qSL("Request not handled"));
}

QUuid IntentClientRequest::requestId() const
{
    return m_id;
}

QString IntentClientRequest::intentId() const
{
    return m_intentId;
}

QString IntentClientRequest::applicationId() const
{
    return m_applicationId;
}

QString IntentClientRequest::requestingApplicationId() const
{
    return m_requestingApplicationId;
}

QVariantMap IntentClientRequest::parameters() const
{
    return m_parameters;
}

bool IntentClientRequest::succeeded() const
{
    return m_succeeded;
}

const QVariantMap IntentClientRequest::result() const
{
    return m_result;
}

QString IntentClientRequest::errorMessage() const
{
    return m_errorMessage;
}

void IntentClientRequest::sendReply(const QVariantMap &result)
{
    //TODO: check that result only contains basic datatypes. convertFromJSVariant() does most of
    //      this already, but doesn't bail out on unconvertible types (yet)

    if (m_direction != Direction::ToApplication)
        return;
    IntentClient *ic = IntentClient::instance();

    if (QThread::currentThread() != ic->thread()) {
        QPointer<IntentClientRequest> that(this);

        ic->metaObject()->invokeMethod(ic, [that, ic, result]()
        { if (that) ic->replyFromApplication(that.data(), result); },
        Qt::QueuedConnection);
    } else {
        ic->replyFromApplication(this, result);
    }
}

void IntentClientRequest::sendErrorReply(const QString &errorMessage)
{
    if (m_direction != Direction::ToApplication)
        return;
    IntentClient *ic = IntentClient::instance();

    if (QThread::currentThread() != ic->thread()) {
        QPointer<IntentClientRequest> that(this);

        ic->metaObject()->invokeMethod(ic, [that, ic, errorMessage]()
        { if (that) ic->errorReplyFromApplication(that.data(), errorMessage); },
        Qt::QueuedConnection);
    } else {
        ic->errorReplyFromApplication(this, errorMessage);
    }
}

void IntentClientRequest::startTimeout(int timeout)
{
    if (timeout <= 0)
        return;

    QTimer::singleShot(timeout, this, [this, timeout]() {
        if (direction() == Direction::ToApplication)
            sendErrorReply(qSL("Intent request to application timed out after %1 ms").arg(timeout));
        else
            setErrorMessage(qSL("No reply received from Intent server after %1 ms").arg(timeout));
    });
}

void IntentClientRequest::connectNotify(const QMetaMethod &signal)
{
    // take care of connects happening after the request is already finished:
    // re-emit the finished signal in this case (this shouldn't happen in practice, but better be
    // safe than sorry)
    if (m_finished && (signal == QMetaMethod::fromSignal(&IntentClientRequest::replyReceived)))
        QMetaObject::invokeMethod(this, &IntentClientRequest::doFinish, Qt::QueuedConnection);
}

IntentClientRequest::IntentClientRequest(Direction direction, const QString &requestingApplicationId,
                                         const QUuid &id, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters)
    : QObject()
    , m_direction(direction)
    , m_id(id)
    , m_intentId(intentId)
    , m_requestingApplicationId(requestingApplicationId)
    , m_applicationId(applicationId)
    , m_parameters(parameters)
{ }

void IntentClientRequest::setRequestId(const QUuid &requestId)
{
    if (m_id != requestId) {
        m_id = requestId;
        emit requestIdChanged();
    }
}

void IntentClientRequest::setResult(const QVariantMap &result)
{
    if (m_result != result) {
        m_result = result;
        m_succeeded = true;
        doFinish();
    }
}

void IntentClientRequest::setErrorMessage(const QString &errorMessage)
{
    if (m_errorMessage != errorMessage) {
        m_errorMessage = errorMessage;
        m_succeeded = false;
        doFinish();
    }
}

void IntentClientRequest::doFinish()
{
    m_finished = true;
    emit replyReceived();
    // We need to disconnect all JS handlers now, because otherwise the request object would
    // never be garbage collected (the signal connections increase the use-counter).
    disconnect(this, &IntentClientRequest::replyReceived, nullptr, nullptr);
}

QT_END_NAMESPACE_AM
