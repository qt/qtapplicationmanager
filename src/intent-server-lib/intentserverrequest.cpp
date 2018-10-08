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

#include <QDebug>
#include "intentserverrequest.h"

QT_BEGIN_NAMESPACE_AM

IntentServerRequest::IntentServerRequest(bool external, const QString &requestingAppId,
                                         const QVector<const Intent *> &intents,
                                         const QVariantMap &parameters)
    : m_id(QUuid::createUuid())
    , m_state(State::ReceivedRequest)
    , m_external(external)
    , m_requestingAppId(requestingAppId)
    , m_intents(intents)
    , m_parameters(parameters)
    , m_actualIntent(intents.size() == 1 ? intents.first() : nullptr)
{ }

IntentServerRequest::State IntentServerRequest::state() const
{
    return m_state;
}

QUuid IntentServerRequest::id() const
{
    return m_id;
}

const Intent *IntentServerRequest::intent() const
{
    return m_actualIntent;
}

QVariantMap IntentServerRequest::parameters() const
{
    return m_parameters;
}

bool IntentServerRequest::isWaiting() const
{
    switch (state()) {
    case State::WaitingForDisambiguation:
    case State::WaitingForApplicationStart:
    case State::WaitingForReplyFromApplication:
        return true;
    default:
        return false;
    }
}

bool IntentServerRequest::hasSucceeded() const
{
    return m_succeeded;
}

QVariantMap IntentServerRequest::result() const
{
    return m_result;
}

void IntentServerRequest::requestFailed(const QString &errorMessage)
{
    m_succeeded = false;
    m_result.clear();
    m_result[qSL("errorMessage")] = errorMessage;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::requestSucceeded(const QVariantMap &result)
{
    m_succeeded = true;
    m_result = result;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::setState(IntentServerRequest::State newState)
{
    m_state = newState;
}

QT_END_NAMESPACE_AM
