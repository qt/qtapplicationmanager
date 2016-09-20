/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QUuid>

#include "global.h"
#include "asynchronoustask.h"

AM_BEGIN_NAMESPACE

AsynchronousTask::AsynchronousTask(QObject *parent)
    : QThread(parent)
    , m_id(QUuid::createUuid().toString())
{
    static int once = qRegisterMetaType<AsynchronousTask::State>();
    Q_UNUSED(once)
}

QString AsynchronousTask::id() const
{
    return m_id;
}

AsynchronousTask::State AsynchronousTask::state() const
{
    return m_state;
}

void AsynchronousTask::setState(AsynchronousTask::State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

QString AsynchronousTask::stateToString(State state)
{
    switch (state) {
    case Queued: return qSL("queued");
    case Executing: return qSL("executing");
    case Failed: return qSL("failed");
    case Finished: return qSL("finished");
    case AwaitingAcknowledge: return qSL("awaitingAcknowledge");
    case Installing: return qSL("installing");
    case CleaningUp: return qSL("cleaningUp");
    default: return QString();
    }
}

bool AsynchronousTask::hasFailed() const
{
    return (m_state == Failed);
}

Error AsynchronousTask::errorCode() const
{
    return m_errorCode;
}

QString AsynchronousTask::errorString() const
{
    return m_errorString;
}


bool AsynchronousTask::cancel()
{
    return false;
}

bool AsynchronousTask::forceCancel()
{
    if (m_state == Queued) {
        setError(Error::Canceled, qSL("canceled"));
        return true;
    }
    return cancel();
}

QString AsynchronousTask::applicationId() const
{
    return m_applicationId;
}

void AsynchronousTask::setError(Error errorCode, const QString &errorString)
{
    m_errorCode = errorCode;
    m_errorString = errorString;
    setState(Failed);
}

void AsynchronousTask::run()
{
    execute();
}

AM_END_NAMESPACE
