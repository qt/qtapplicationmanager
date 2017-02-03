/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include "processmonitor.h"
#include "processmonitor_p.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

ProcessMonitor::ProcessMonitor(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new ProcessMonitorPrivate(this))
{
    Q_D(ProcessMonitor);

    d->roles.insert(MemVirtual, "memoryVirtual");
    d->roles.insert(MemRss, "memoryRss");
    d->roles.insert(MemPss, "memoryPss");
    d->roles.insert(CpuLoad, "cpuLoad");

    d->resetModel();
}

ProcessMonitor::~ProcessMonitor()
{
    Q_D(ProcessMonitor);

    delete d;
}

int ProcessMonitor::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    Q_D(const ProcessMonitor);

    return d->modelData.size();
}

void ProcessMonitor::setCount(int count)
{
    Q_D(ProcessMonitor);

    if (count != d->count) {
        count = qMax(2, count);
        d->updateModelCount(count);
        emit d->newCount(count);
        emit countChanged(count);
    }
}

int ProcessMonitor::count() const
{
    Q_D(const ProcessMonitor);

    return d->count;
}

qint64 ProcessMonitor::processId() const
{
    Q_D(const ProcessMonitor);

    return d->pid;
}


QString ProcessMonitor::applicationId() const
{
    Q_D(const ProcessMonitor);

    return d->appId;
}

void ProcessMonitor::setApplicationId(const QString &appId)
{
    Q_D(ProcessMonitor);

    if (d->appId != appId || d->appId.isNull()) {
        d->appId = appId;
        d->resetModel();
        d->determinePid();
        if (!appId.isEmpty() && ApplicationManager::instance()->indexOfApplication(appId) < 0)
            qCWarning(LogSystem) << "ProcessMonitor: invalid application ID:" << appId;
        emit applicationIdChanged(appId);
    }
}

void ProcessMonitor::setReportingInterval(int intervalInMSec)
{
    Q_D(ProcessMonitor);

    if (d->reportingInterval != intervalInMSec && intervalInMSec > 0) {
        d->setupInterval(intervalInMSec);
        d->resetModel();
        emit reportingIntervalChanged(intervalInMSec);
    }
}

int ProcessMonitor::reportingInterval() const
{
    Q_D(const ProcessMonitor);

    return d->reportingInterval;
}

bool ProcessMonitor::isMemoryReportingEnabled() const
{
    Q_D(const ProcessMonitor);

    return d->reportMemory;
}

void ProcessMonitor::setMemoryReportingEnabled(bool enabled)
{
    Q_D(ProcessMonitor);

    if (enabled != d->reportMemory) {
        d->reportMemory = enabled;
        d->memTail = enabled ? 0 : d->count;
        d->setupInterval();
        emit d->newReadMem(enabled);
        emit memoryReportingEnabledChanged();
    }
}

bool ProcessMonitor::isCpuLoadReportingEnabled() const
{
    Q_D(const ProcessMonitor);
    return d->reportCpu;
}

void ProcessMonitor::setCpuLoadReportingEnabled(bool enabled)
{
    Q_D(ProcessMonitor);

    if (enabled != d->reportCpu) {
        d->reportCpu = enabled;
        d->cpuTail = enabled ? 0: d->count;
        d->setupInterval();
        emit d->newReadCpu(enabled);
        emit cpuLoadReportingEnabledChanged();
    }
}

QVariant ProcessMonitor::data(const QModelIndex &index, int role) const
{
    Q_D(const ProcessMonitor);

    if (!index.isValid() || index.row() < 0 || index.row() >= d->modelData.size())
        return QVariant();

    const ProcessMonitorPrivate::ModelData &reading = d->modelDataForRow(index.row());

    switch (role) {
    case MemVirtual:
        return reading.vm;
    case MemRss:
        return reading.rss;
    case MemPss:
        return reading.pss;
    case CpuLoad:
        return reading.cpuLoad;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ProcessMonitor::roleNames() const
{
    Q_D(const ProcessMonitor);

    return d->roles;
}

QVariantMap ProcessMonitor::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogSystem) << "ProcessMonitor: invalid row:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        map.insert(qL1S(it.value()), data(index(row), it.key()));
    }

    return map;
}

QT_END_NAMESPACE_AM
