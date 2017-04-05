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


/*!
    \qmltype ProcessMonitor
    \inqmlmodule QtApplicationManager
    \brief A type for monitoring process resource usage

    The ProcessMonitor type provides statistics about the resource usage for a process known to the
    application-manager. Currently, CPU load and memory usage can be monitored. This type is
    available in the system-UI only.

    The ProcessMonitor is dedicated to Linux in particular, since this is currently the only OS
    that supports multi-process mode. Other OS's are supported only rudimenatary.

    Here is an example, how the ProcessMonitor can be used:

    \qml
    import QtApplicationManager 1.0

    ProcessMonitor {
        applicationId: ""
        reportingInterval: 1000
        memoryReportingEnabled: true

        onMemoryReportingChanged: {
            console.log("Total PSS: " + (memoryPss.total / 1e6).toFixed(0) + " MB");
        }
    }
    \endqml

    The type is derived from \c QAbstractListModel, so it can be used directly as a model in an
    appropriate view.

    \target role-names
    Here is the list of roles that the model provides:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c cpuLoad
        \target cpuLoad-role
        \li real
        \li The process's CPU utilization during the last reporting interval. A value of 0 means
            that the process was idle, a value of 1 means it fully used the equivalent of one core
            (which may be split over several ones).
    \row
        \li \c memoryVirtual
        \target memoryVirtual-role
        \li var
        \li A map of the process's virtual memory usage. See below for a list of supported keys.
            The total amount of virtual memory is provided through \c memoryVirtual.total for
            example.
    \row
        \li \c memoryRss
        \li var
        \li A map of the process's RSS (Resident Set Size) memory usage. This is the amount of
            memory that is actually mapped to physical RAM. See below for a list of supported
            keys.
    \row
        \li \c memoryPss
        \li var
        \li A map of the process's PSS (Proportional Set Size) memory usage. This is the
            proportional share of the RSS value above. For instance if two processes share 2 MB
            the RSS value will be 2 MB for each process and the PSS value 1 MB for each process.
            As the name implies, the code section of shared libraries is generally shared between
            processes. Memory may also be shared by other means provided by the OS (e.g. through
            \c mmap on Linux). See below for a list of supported keys.
    \endtable

    These are the supported keys in the memory maps:

    \table
    \header
        \li Key
        \li Description
    \row
        \li total
        \li The amount of memory used in total in bytes.
    \row
        \li text
        \li The amount of memory used by the code section in bytes.
    \row
        \li heap
        \li The amount of memory used by the heap in bytes. This is private, dynamically allocated
            memory (for example through \c malloc or \c mmap on Linux).
    \endtable

    \note The model will be updated each \l reportingInterval milliseconds. Notet that the roles
          will only be populated, if the corresponding reporting parts have been enabled.
*/

/*!
    \qmlproperty int ProcessMonitor::count

    This property holds the number of reading points that will be kept in the model. The minimum
    value that can be set is 2 and the default value is 10.
*/

/*!
    \qmlproperty int ProcessMonitor::processId
    \readonly

    This property holds the OS specific process identifier (PID) that is monitored. This can be
    used by external tools for example. The property is 0, if there is no process associated with
    the \l applicationId. In particular, if the application-manager runs in single-process mode,
    only the system-UI (identified by an empty \l applicationId) will have an associated process.
*/

/*!
    \qmlproperty string ProcessMonitor::applicationId

    The ID of the application that will be monitored. It must be one of the ID's known to the
    application-manager (\l ApplicationManager::applicationIds provides a list of valid IDs). There
    is one exception: if the ID is set to an empty string, the system-UI process will be monitored.
    Setting a new value will reset the model.
*/

/*!
    \qmlproperty int ProcessMonitor::reportingInterval

    This property holds the interval in milliseconds between reporting updates. Note, that
    reporting will only start once this property is set. Setting a new value will reset the model.
    Valid values must be greater than zero.

    At least one of the reporting parts must be enabled to start the reporting.

    \sa cpuLoadReportingEnabled
    \sa memoryReportingEnabled
*/

/*!
    \qmlproperty bool ProcessMonitor::cpuLoadReportingEnabled

    A boolean value that determines whether periodic CPU load reporting is enabled.
*/

/*!
    \qmlproperty bool ProcessMonitor::memoryReportingEnabled

    A boolean value that determines whether periodic memory reporting is enabled.
*/

/*!
    \qmlsignal ProcessMonitor::cpuLoadReportingChanged(real load)

    This signal is emitted periodically when CPU load reporting is enabled. The frequency is
    defined by \l reportingInterval. The \a load parameter indicates the CPU utilization. Details
    can be found in the description of the \l {cpuLoad-role} {cpuLoad} model role above.

    \sa cpuLoadReportingEnabled
    \sa reportingInterval
*/

/*!
    \qmlsignal ProcessMonitor::memoryReportingChanged(var memoryVritual, var memoryRss
                                                                       , var memoryPss);

    This signal is emitted periodically when memory reporting is enabled. The frequency is defined
    by \l reportingInterval. The parameters provide the same information as the model roles with
    the same name described \l {memoryVirtual-role}{above}.

    \sa memoryReportingEnabled
    \sa reportingInterval
*/


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

/*!
    \qmlmethod object ProcessMonitor::get(int index)

    Returns the model data for the reading point identified by \a index as a JavaScript object.
    See the \l {role-names} for the expected object elements. The \a index must be in the range
    [0, \l count), returns an empty object if it is invalid.
*/
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
