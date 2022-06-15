// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
