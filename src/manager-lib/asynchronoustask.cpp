// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
