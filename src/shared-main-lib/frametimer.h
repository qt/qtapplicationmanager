// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtAppManCommon/global.h>
#include <limits>


QT_BEGIN_NAMESPACE_AM

class FrameTimerImpl;

class FrameTimer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/FrameTimer 2.0")

    Q_PROPERTY(qreal averageFps READ averageFps NOTIFY updated FINAL)
    Q_PROPERTY(qreal minimumFps READ minimumFps NOTIFY updated FINAL)
    Q_PROPERTY(qreal maximumFps READ maximumFps NOTIFY updated FINAL)
    Q_PROPERTY(qreal jitterFps READ jitterFps NOTIFY updated FINAL)

    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged FINAL)

    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    FrameTimer(QObject *parent = nullptr);
    ~FrameTimer() override;

    QStringList roleNames() const;

    Q_INVOKABLE void update();
    Q_SIGNAL void updated();

    qreal averageFps() const;
    qreal minimumFps() const;
    qreal maximumFps() const;
    qreal jitterFps() const;

    QObject *window() const;
    void setWindow(QObject *window);
    Q_SIGNAL void windowChanged();

    bool running() const;
    void setRunning(bool running);
    Q_SIGNAL void runningChanged();

    int interval() const;
    void setInterval(int interval);
    Q_SIGNAL void intervalChanged();

    void reportFrameSwap();

    FrameTimerImpl *implementation();

protected:
    std::unique_ptr<FrameTimerImpl> m_impl;

private:
    QPointer<QObject> m_window;

    int m_count = 0;
    int m_sum = 0;
    int m_min = std::numeric_limits<int>::max();
    int m_max = 0;
    qreal m_jitter = 0.0;

    QElapsedTimer m_timer;

    QTimer m_updateTimer;

    qreal m_averageFps;
    qreal m_minimumFps;
    qreal m_maximumFps;
    qreal m_jitterFps;

    QMetaObject::Connection m_frameSwapConnection;

    static const int IdealFrameTime = 16667; // usec - could be made configurable via an env variable
    static const qreal MicrosInSec;
};

QT_END_NAMESPACE_AM
