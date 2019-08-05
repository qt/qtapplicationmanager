/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#if !defined(AM_HEADLESS)

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtAppManCommon/global.h>
#include <limits>

#if defined(AM_MULTI_PROCESS)
#  include <QtWaylandCompositor/QWaylandQuickSurface>
#endif

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

#endif // !AM_HEADLESS
