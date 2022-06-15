// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtAppManCommon/global.h>
#include <limits>


QT_BEGIN_NAMESPACE_AM

class FrameTimer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/FrameTimer 2.0")

    Q_PROPERTY(qreal averageFps READ averageFps NOTIFY updated)
    Q_PROPERTY(qreal minimumFps READ minimumFps NOTIFY updated)
    Q_PROPERTY(qreal maximumFps READ maximumFps NOTIFY updated)
    Q_PROPERTY(qreal jitterFps READ jitterFps NOTIFY updated)

    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)

    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT)

public:
    FrameTimer(QObject *parent = nullptr);

    QStringList roleNames() const;

    Q_INVOKABLE void update();

    qreal averageFps() const;
    qreal minimumFps() const;
    qreal maximumFps() const;
    qreal jitterFps() const;

    QObject *window() const;
    void setWindow(QObject *value);

    bool running() const;
    void setRunning(bool value);

    int interval() const;
    void setInterval(int value);

signals:
    void updated();
    void intervalChanged();
    void runningChanged();
    void windowChanged();

protected slots:
    void newFrame();

protected:
    virtual bool connectToAppManWindow();
    virtual void disconnectFromAppManWindow();

    QPointer<QObject> m_window;

private:
    void reset();
    bool connectToQuickWindow();

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

    static const int IdealFrameTime = 16667; // usec - could be made configurable via an env variable
    static const qreal MicrosInSec;
};

QT_END_NAMESPACE_AM
