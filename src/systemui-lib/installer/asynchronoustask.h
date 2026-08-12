// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef ASYNCHRONOUSTASK_H
#define ASYNCHRONOUSTASK_H

#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QAtomicInteger>
#include <QtAppManSystemUI/qtappmansystemuiglobal.h>


QT_BEGIN_NAMESPACE_AM

class Q_APPMANSYSTEMUI_EXPORT AsynchronousTask : public QThread
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

    enum class Origin {
        Invalid,
        ApplicationDeveloper,
        SystemDeveloper,
        SystemUI
    };
    Q_ENUM(Origin)

    AsynchronousTask(Origin origin, QObject *parent = nullptr);

    QString id() const;

    TaskState state() const;
    void setState(TaskState state);

    Origin origin() const;

    QString errorString() const;

    virtual bool cancel();
    bool forceCancel(); // will always work in Queued state
    bool wasCanceled() const;

    QString packageId() const; // convenience

    virtual bool preExecute();
    virtual bool postExecute();

Q_SIGNALS:
    void stateChanged(QtAM::AsynchronousTask::TaskState newState);
    void progress(qreal p);

protected:
    void setError(const QString &errorString);
    virtual void execute() = 0;
    void run() final;

protected:
    mutable QMutex m_mutex;

    QString m_id;
    QString m_packageId;
    QString m_errorString;
    TaskState m_state = Queued;
    Origin m_origin = Origin::ApplicationDeveloper;
    QAtomicInteger<bool> m_canceled = false; // atomic for easy access in "busy" wait loops
};


QT_END_NAMESPACE_AM

#endif // ASYNCHRONOUSTASK_H
