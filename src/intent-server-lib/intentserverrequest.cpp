/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include "intentserverrequest.h"

QT_BEGIN_NAMESPACE_AM

IntentServerRequest::IntentServerRequest(const QString &requestingApplicationId, const QString &intentId,
                                         const QVector<Intent> &potentialIntents,
                                         const QVariantMap &parameters)
    : m_id(QUuid::createUuid())
    , m_state(State::ReceivedRequest)
    , m_intentId(intentId)
    , m_requestingApplicationId(requestingApplicationId)
    , m_potentialIntents(potentialIntents)
    , m_parameters(parameters)
{
    Q_ASSERT(!potentialIntents.isEmpty());

    if (potentialIntents.size() == 1)
        setHandlingApplicationId(potentialIntents.first().applicationId());
}

IntentServerRequest::State IntentServerRequest::state() const
{
    return m_state;
}

QString IntentServerRequest::intentId() const
{
    return m_intentId;
}

QUuid IntentServerRequest::requestId() const
{
    return m_id;
}

QString IntentServerRequest::requestingApplicationId() const
{
    return m_requestingApplicationId;
}

QString IntentServerRequest::handlingApplicationId() const
{
    return m_handlingApplicationId;
}

QVector<Intent> IntentServerRequest::potentialIntents() const
{
    return m_potentialIntents;
}

QVariantMap IntentServerRequest::parameters() const
{
    return m_parameters;
}

bool IntentServerRequest::succeeded() const
{
    return m_succeeded;
}

QVariantMap IntentServerRequest::result() const
{
    return m_result;
}

void IntentServerRequest::setRequestFailed(const QString &errorMessage)
{
    m_succeeded = false;
    m_result.clear();
    m_result[qSL("errorMessage")] = errorMessage;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::setRequestSucceeded(const QVariantMap &result)
{
    m_succeeded = true;
    m_result = result;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::setState(IntentServerRequest::State newState)
{
    m_state = newState;
}

void IntentServerRequest::setHandlingApplicationId(const QString &applicationId)
{
    m_handlingApplicationId = applicationId;
}

QT_END_NAMESPACE_AM
