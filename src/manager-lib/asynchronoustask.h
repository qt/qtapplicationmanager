// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef ASYNCHRONOUSTASK_H
#define ASYNCHRONOUSTASK_H

#include <QtCore/QThread>
#include <QtCore/QMutex>

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

    Error errorCode() const;
    QString errorString() const;

    virtual bool cancel();
    bool forceCancel(); // will always work in Queued state

    QString packageId() const; // convenience

    virtual bool preExecute();
    virtual bool postExecute();

Q_SIGNALS:
    void stateChanged(QtAM::AsynchronousTask::TaskState newState);
    void progress(qreal p);

protected:
    void setError(Error errorCode, const QString &errorString);
    virtual void execute() = 0;
    void run() override final;

protected:
    mutable QMutex m_mutex;

    QString m_id;
    QString m_packageId;
    TaskState m_state = Queued;
    Error m_errorCode = Error::None;
    QString m_errorString;
};


QT_END_NAMESPACE_AM

#endif // ASYNCHRONOUSTASK_H
