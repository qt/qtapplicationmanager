// Copyright (C) 2026 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSSTATUS_P_H
#define PROCESSSTATUS_P_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <private/qobject_p.h>

#include <QtAppManSystemUI/application.h>

#include "processstatus.h"

#if defined(Q_OS_LINUX)
#  include <QtCore/QFile>
#endif

QT_FORWARD_DECLARE_CLASS(QThread)

QT_BEGIN_NAMESPACE_AM

class Q_AUTOTEST_EXPORT ProcessReader : public QObject
{
    Q_OBJECT

public:
    QMutex mutex;
    qreal cpuLoad = 0.0;
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

public Q_SLOTS:
    void update();
    void setProcessId(qint64 pid);
    void enableMemoryReporting(bool enabled);

Q_SIGNALS:
    void updated();

private:
    void openCpuLoad();
    qreal readCpuLoad();
    bool readMemory(Memory &mem);

#if defined(Q_OS_LINUX)
    bool readSmaps(const QByteArray &smapsFile, Memory &mem);

    QFile m_statFile;
    QElapsedTimer m_elapsedTime;
    quint64 m_lastCpuUsage = 0.0;
#endif

    qint64 m_pid = 0;
    bool m_memoryReportingEnabled = true;
};

class ProcessStatusPrivate : public QObjectPrivate
{
public:
    void fetchReadings();
    void determinePid();

    QString m_appId;
    qint64 m_pid = 0;

    qreal m_cpuLoad = 0;
    QVariantMap m_memoryVirtual;
    QVariantMap m_memoryRss;
    QVariantMap m_memoryPss;
    bool m_memoryReportingEnabled = true;

    QPointer<Application> m_application;

    bool m_pendingUpdate = false;
    ProcessReader *m_reader;

    static QThread *s_workerThread;
    static int s_instanceCount;

    Q_DECLARE_PUBLIC(ProcessStatus)
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // PROCESSSTATUS_P_H
