/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QByteArray>
#include <QPair>
#include <QElapsedTimer>
#include <QObject>

QT_FORWARD_DECLARE_CLASS(QSocketNotifier)

class SysFsReader
{
public:
    SysFsReader(const char *path, int maxRead = 2048);
    ~SysFsReader();

    bool open();
    int handle() const;
    QByteArray readValue() const;

private:
    int m_fd = -1;
    QByteArray m_path;
    int m_maxRead;

    Q_DISABLE_COPY(SysFsReader)
};


class CpuReader : public SysFsReader
{
public:
    CpuReader();

    QPair<int, qreal> readLoadValue();

private:
    QElapsedTimer m_lastCheck;
    qint64 m_lastIdle = 0;
    qint64 m_lastTotal = 0;
    qreal m_load = 1;
};

class MemoryReader : public SysFsReader
{
public:
    MemoryReader();

    quint64 totalValue() const;
    quint64 readUsedValue() const;

private:
    quint64 m_totalValue;
};

class IoReader : public SysFsReader
{
public:
    IoReader(const char *device);

    QPair<int, qreal> readLoadValue();

private:
    QElapsedTimer m_lastCheck;
    qint64 m_lastIoTime = 0;
    qreal m_load = 1;
};

class MemoryThreshold : public QObject
{
    Q_OBJECT

public:
    MemoryThreshold(const QList<qreal> &thresholds);
    ~MemoryThreshold();
    QList<qreal> thresholdPercentages() const;

    bool isEnabled() const;
    bool setEnabled(bool enabled);

signals:
    void thresholdTriggered();

private slots:
    void readEventFd();

private:
    bool m_initialized = false;
    bool m_enabled = false;
    QList<qreal> m_thresholds;
    int m_eventFd = -1;
    int m_controlFd = -1;
    int m_usageFd = -1;
    QSocketNotifier *m_notifier = 0;
};
