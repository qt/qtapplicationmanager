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

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUuid>
#include <QVector>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentServer;
class Intent;

class IntentServerRequest
{
    Q_GADGET

public:
    IntentServerRequest(bool external, const QString &requestingAppId, const QVector<const Intent *> &intents,
                        const QVariantMap &parameters);

    enum class State {
        ReceivedRequest,
        WaitingForDisambiguation,
        Disambiguated,
        WaitingForApplicationStart,
        StartedApplication,
        WaitingForReplyFromApplication,
        ReceivedReplyFromApplication,
    };

    Q_ENUM(State)

    State state() const;
    QUuid id() const;
    const QtAM::Intent *intent() const;
    QVariantMap parameters() const;
    bool isWaiting() const;
    bool hasSucceeded() const;
    QVariantMap result() const;

    void requestFailed(const QString &errorMessage);
    void requestSucceeded(const QVariantMap &result);

private:
    void setState(State newState);

private:
    QUuid m_id;
    State m_state;
    bool m_external;
    bool m_succeeded = false;
    QString m_requestingAppId;
    QVector<const Intent *> m_intents;
    QVariantMap m_parameters;
    QVariantMap m_result;
    const Intent *m_actualIntent;

    friend class IntentServer;
};

QT_END_NAMESPACE_AM
