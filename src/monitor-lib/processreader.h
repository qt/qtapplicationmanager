// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QMutex>
#include <QElapsedTimer>
#include <QObject>

#include <QtAppManCommon/global.h>

#if defined(Q_OS_LINUX)
#  include <memory>
#  include <QtAppManMonitor/sysfsreader.h>
#endif

QT_BEGIN_NAMESPACE_AM

class ProcessReader : public QObject {
    Q_OBJECT

public:
    QMutex mutex;
    qreal cpuLoad;
    struct Memory {
        quint32 totalVm = 0;
        quint32 totalRss = 0;
        quint32 totalPss = 0;
        quint32 textVm = 0;
        quint32 textRss = 0;
        quint32 textPss = 0;
        quint32 heapVm = 0;
        quint32 heapRss = 0;
        quint32 heapPss = 0;
    } memory;

#if defined(Q_OS_LINUX)
    // solely for testing purposes
    bool testReadSmaps(const QByteArray &smapsFile);
#endif

public slots:
    void update();
    void setProcessId(qint64 pid);
    void enableMemoryReporting(bool enabled);

signals:
    void updated();

private:
    void openCpuLoad();
    qreal readCpuLoad();
    bool readMemory(Memory &mem);

#if defined(Q_OS_LINUX)
    bool readSmaps(const QByteArray &smapsFile, Memory &mem);

    std::unique_ptr<SysFsReader> m_statReader;
    QElapsedTimer m_elapsedTime;
    quint64 m_lastCpuUsage = 0.0;
#endif

    qint64 m_pid = 0;
    bool m_memoryReportingEnabled = true;
};

QT_END_NAMESPACE_AM
