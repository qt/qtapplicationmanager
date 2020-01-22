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

#include <QMutex>
#include <QElapsedTimer>
#include <QObject>

#include <QtAppManCommon/global.h>

#if defined(Q_OS_LINUX)
#  include <QScopedPointer>
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

    QScopedPointer<SysFsReader> m_statReader;
#endif
    QElapsedTimer m_elapsedTime;
    quint64 m_lastCpuUsage = 0.0;

    qint64 m_pid = 0;
    bool m_memoryReportingEnabled = true;
};

QT_END_NAMESPACE_AM
