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

#include <QByteArray>
#include <QPair>
#include <QElapsedTimer>
#include <QObject>
#include <QtAppManCommon/global.h>

#if defined(Q_OS_LINUX)
#  include <QScopedPointer>
#  include <QtAppManMonitor/sysfsreader.h>
QT_FORWARD_DECLARE_CLASS(QSocketNotifier)
#endif

QT_BEGIN_NAMESPACE_AM

class CpuReader
{
public:
    CpuReader();
    qreal readLoadValue();

private:
    qint64 m_lastIdle = 0;
    qint64 m_lastTotal = 0;
    qreal m_load = 1;
#if defined(Q_OS_LINUX)
    static QScopedPointer<SysFsReader> s_sysFs;
#endif
    Q_DISABLE_COPY(CpuReader)
};

class GpuVendor {
public:
    enum Vendor {
        Undefined = 0, // didn't try to determine the vendor yet
        Unsupported,
        Intel,
        Nvidia
    };
    static Vendor get();
private:
    static void fetch();
    static Vendor s_vendor;
};

class GpuTool;

class GpuReader
{
public:
    GpuReader();
    void setActive(bool enabled);
    bool isActive() const;
    qreal readLoadValue();

private:
#if defined(Q_OS_LINUX)
    static GpuTool *s_gpuToolProcess;
#endif
    Q_DISABLE_COPY(GpuReader)
};


class MemoryReader
{
public:
    MemoryReader();
#if defined(Q_OS_LINUX)
    explicit MemoryReader(const QString &groupPath);
    quint64 groupLimit();
#endif
    quint64 totalValue() const;
    quint64 readUsedValue() const;

private:
    static quint64 s_totalValue;
#if defined(Q_OS_LINUX)
    QScopedPointer<SysFsReader> m_sysFs;
    const QString m_groupPath;
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    static int s_pageSize;
#endif
    Q_DISABLE_COPY(MemoryReader)
};

class IoReader
{
public:
    IoReader(const char *device);
    ~IoReader();
    qreal readLoadValue();

private:
#if defined(Q_OS_LINUX)
    QElapsedTimer m_lastCheck;
    qint64 m_lastIoTime = 0;
    qreal m_load = 1;
    QScopedPointer<SysFsReader> m_sysFs;
#endif
    Q_DISABLE_COPY(IoReader)
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
#if defined(Q_OS_LINUX)
    bool setEnabled(bool enabled, const QString &groupPath, MemoryReader *reader);
#endif

signals:
    void thresholdTriggered();

private:
    bool m_initialized = false;
    bool m_enabled = false;
    QList<qreal> m_thresholds;

#if defined(Q_OS_LINUX)
private slots:
    void readEventFd();

private:
    int m_eventFd = -1;
    int m_controlFd = -1;
    int m_usageFd = -1;
    QSocketNotifier *m_notifier = nullptr;
#endif
};

class MemoryWatcher : public QObject
{
    Q_OBJECT
public:
    MemoryWatcher(QObject *parent);

    void setThresholds(qreal warning, qreal critical);
    bool startWatching(const QString &groupPath = QString());
    void checkMemoryConsumption();

signals:
    void memoryLow();
    void memoryCritical();

private:
    qreal m_warning = 75.0;
    qreal m_critical = 90.0;
    qreal m_memLimit;
    bool hasMemoryLowWarning = false;
    bool hasMemoryCriticalWarning = false;
    QScopedPointer<MemoryThreshold> m_threshold;
    QScopedPointer<MemoryReader> m_reader;
};

#if defined(Q_OS_LINUX)
// Parses the file /proc/$PID/cgroup, returning a map groupName->path
// eg: map["memory"] == "/user.slice"
QMap<QByteArray, QByteArray> fetchCGroupProcessInfo(qint64 pid);

// Where the filesystem root directory is located. This exists solely to enable testing.
// In production it's naturally "/"
extern QString g_systemRootDir;
#endif

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
