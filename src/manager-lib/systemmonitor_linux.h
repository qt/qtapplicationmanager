/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
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
