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

#pragma once

#include <QThread>
#include <QMutex>

#include <QtAppManCommon/error.h>

QT_BEGIN_NAMESPACE_AM

class AsynchronousTask : public QThread
{
    Q_OBJECT

public:
    enum TaskState
    {
        Invalid,
        Queued,
        Executing,
        Failed,
        Finished,

        // installation task only
        AwaitingAcknowledge,
        Installing,
        CleaningUp
    };
    Q_ENUM(TaskState)

    AsynchronousTask(QObject *parent = nullptr);

    QString id() const;

    TaskState state() const;
    void setState(TaskState state);

    bool hasFailed() const;
    Error errorCode() const;
    QString errorString() const;

    virtual bool cancel();
    bool forceCancel(); // will always work in Queued state

    QString packageId() const; // convenience

    virtual bool preExecute();
    virtual bool postExecute();

signals:
    void stateChanged(QT_PREPEND_NAMESPACE_AM(AsynchronousTask::TaskState) newState);
    void progress(qreal p);

protected:
    void setError(Error errorCode, const QString &errorString);
    virtual void execute() = 0;
    void run() override final;

protected:
    QMutex m_mutex;

    QString m_id;
    QString m_packageId;
    TaskState m_state = Queued;
    Error m_errorCode = Error::None;
    QString m_errorString;
};


QT_END_NAMESPACE_AM
