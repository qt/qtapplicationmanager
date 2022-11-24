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

#include <QUuid>

#include "global.h"
#include "asynchronoustask.h"

QT_BEGIN_NAMESPACE_AM

AsynchronousTask::AsynchronousTask(QObject *parent)
    : QThread(parent)
    , m_id(QUuid::createUuid().toString())
{
    static int once = qRegisterMetaType<AsynchronousTask::TaskState>();
    Q_UNUSED(once)
}

QString AsynchronousTask::id() const
{
    return m_id;
}

AsynchronousTask::TaskState AsynchronousTask::state() const
{
    return m_state;
}

void AsynchronousTask::setState(AsynchronousTask::TaskState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
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

QString AsynchronousTask::packageId() const
{
    return m_packageId;
}

bool AsynchronousTask::preExecute()
{
    return true;
}

bool AsynchronousTask::postExecute()
{
    return true;
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

QT_END_NAMESPACE_AM

#include "moc_asynchronoustask.cpp"
