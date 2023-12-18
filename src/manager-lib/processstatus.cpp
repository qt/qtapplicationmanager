// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "processstatus.h"

#include <QCoreApplication>
#include <QMutexLocker>
#include <QtQml/qqmlinfo.h>


#include "abstractruntime.h"
#include "applicationmanager.h"
#include "logging.h"

#include <limits>

/*!
    \qmltype ProcessStatus
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-instantiable
    \brief Provides information about the status of an application process.

    ProcessStatus provides information about the process of a given application.

    You can use it alongside a Timer, for instance, to periodically query the status of an
    application process.

    \qml
    import QtQuick
    import QtApplicationManager
    import QtApplicationManager.SystemUI

    Item {
        id: root
        property var application: ApplicationManager.get(0)
        ...
        ProcessStatus {
            id: processStatus
            applicationId: root.application.id
        }
        Timer {
            interval: 1000
            running: root.visible && root.application.runState === Am.Running
            repeat: true
            onTriggered: processStatus.update()
        }
        Text {
            text: "PSS.total: " + (processStatus.memoryPss.total / 1e6).toFixed(0) + " MB"
        }
    }
    \endqml

    You can also use this type as a data source for MonitorModel, if you want to plot its previous
    values over time:

    \qml
    import QtQuick
    import QtApplicationManager
    import QtApplicationManager.SystemUI
    ...
    MonitorModel {
        running: true
        ProcessStatus {
            applicationId: "some.app.id"
        }
    }
    \endqml

    \target supported-keys
    The following are the keys supported in the memory properties (\c memoryVirtual, \c memoryRss,
    and \c memoryPss):

    \table
    \header
        \li Key
        \li Description
    \row
        \li \c total
        \li The total amount of memory used, in bytes.
    \row
        \li \c text
        \li The amount of memory used by the code section, in bytes.
    \row
        \li \c heap
        \li The amount of memory used by the heap, in bytes. The heap is private, dynamically
            allocated memory, for example through \c malloc or \c mmap on Linux.
    \endtable
*/

/*!
    \qmlsignal ProcessStatus::memoryReportingChanged(var memoryVirtual, var memoryRss, var memoryPss)

    This signal is emitted after \l{ProcessStatus::update()}{update()} has been called and the
    memory usage values have been refreshed. Each of the arguments \a memoryVirtual, \a memoryRss
    and \a memoryPss is an JavaScript object with the available properties listed in the table
    \l{supported-keys}{above}.
*/


QT_USE_NAMESPACE_AM

QThread *ProcessStatus::m_workerThread = nullptr;
int ProcessStatus::m_instanceCount = 0;

ProcessStatus::ProcessStatus(QObject *parent)
    : QObject(parent)
{
    if (m_instanceCount == 0) {
        m_workerThread = new QThread;
        m_workerThread->setObjectName(qSL("QtAM-ProcessStatus"));
        m_workerThread->start();
    }
    ++m_instanceCount;

    m_reader = new ProcessReader;
    m_reader->moveToThread(m_workerThread);

    connect(m_reader, &ProcessReader::updated, this, [this]() {
        fetchReadings();
        emit cpuLoadChanged();
        emit memoryReportingChanged(m_memoryVirtual, m_memoryRss, m_memoryPss);
        m_pendingUpdate = false;
    });
    connect(this, &ProcessStatus::processIdChanged, m_reader, &ProcessReader::setProcessId);
    connect(this, &ProcessStatus::memoryReportingEnabledChanged, m_reader, &ProcessReader::enableMemoryReporting);
}

ProcessStatus::~ProcessStatus()
{
    m_reader->deleteLater();

    --m_instanceCount;
    if (m_instanceCount == 0) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
}

/*!
    \qmlmethod ProcessStatus::update

    Updates the cpuLoad, memoryVirtual, memoryRss, and memoryPss properties.
*/
void ProcessStatus::update()
{
    if (!m_pendingUpdate) {
        m_pendingUpdate = true;
        QMetaObject::invokeMethod(m_reader, &ProcessReader::update);
    }
}

/*!
    \qmlproperty string ProcessStatus::applicationId

    Holds the \l{ApplicationObject::id}{ID} of the \l{ApplicationObject}{application} whose process
    is to be monitored. This ID must be one that is known to the application manager
    (\l{ApplicationManager::applicationIds}{applicationIds()} provides a list of valid IDs). There
    is one exception: if you want to monitor the System UI's process, set the ID to an empty
    string. In single-process mode, the System UI process is the only valid process, since all
    applications run within this process.

    \sa ApplicationObject
*/
QString ProcessStatus::applicationId() const
{
    return m_appId;
}

void ProcessStatus::setApplicationId(const QString &appId)
{
    if (m_appId != appId || m_appId.isNull()) {
        if (m_application) {
            disconnect(m_application, nullptr, this, nullptr);
            m_application = nullptr;
        }
        m_appId = appId;
        if (!appId.isEmpty()) {
            int appIndex = ApplicationManager::instance()->indexOfApplication(appId);
            if (appIndex < 0) {
                qmlWarning(this) << "Invalid application ID:" << appId;
            } else {
                m_application = ApplicationManager::instance()->application(appIndex);
                connect(m_application.data(), &Application::runStateChanged, this, &ProcessStatus::onRunStateChanged);
            }
        }
        determinePid();
        emit applicationIdChanged(appId);
    }
}

void ProcessStatus::onRunStateChanged(Am::RunState state)
{
    if (state == Am::Running || state == Am::NotRunning)
        determinePid();
}

void ProcessStatus::determinePid()
{
    qint64 newId;
    if (m_appId.isEmpty()) {
        newId = QCoreApplication::applicationPid();
    } else {
        Q_ASSERT(m_application);
        if (ApplicationManager::instance()->isSingleProcess())
            newId = 0;
        else
            newId = m_application->currentRuntime() ? m_application->currentRuntime()->applicationProcessId() : 0;
    }

    if (newId != m_pid) {
        m_pid = newId;
        emit processIdChanged(m_pid);
    }
}

/*!
    \qmlproperty int ProcessStatus::processId
    \readonly

    This property holds the OS-specific process identifier (PID) that is monitored. It can be
    used by external tools, for example. The property is 0, if there is no process associated with
    the \l applicationId. In particular, if the application manager runs in single-process mode,
    only the System UI has an associated process. The System UI is always identified by an empty
    \l applicationId.
*/
qint64 ProcessStatus::processId() const
{
    return m_pid;
}

/*!
    \qmlproperty real ProcessStatus::cpuLoad
    \readonly

    This property holds the process' CPU utilization during the previous measurement interval, when
    update() was last called. A value of 0 means the process was idle; a value of 1 means the process used
    the equivalent of one core, which may be split across several cores.

    \sa ProcessStatus::update
*/
qreal ProcessStatus::cpuLoad()
{
    return m_cpuLoad;
}

void ProcessStatus::fetchReadings()
{
    QMutexLocker locker(&m_reader->mutex);

    m_cpuLoad = m_reader->cpuLoad;

    // Although smaps claims to report kB it's actually KiB (2^10 = 1024 Bytes)
    m_memoryVirtual[qSL("total")] = static_cast<quint64>(m_reader->memory.totalVm) << 10;
    m_memoryVirtual[qSL("text")] = static_cast<quint64>(m_reader->memory.textVm) << 10;
    m_memoryVirtual[qSL("heap")] = static_cast<quint64>(m_reader->memory.heapVm) << 10;
    m_memoryRss[qSL("total")] = static_cast<quint64>(m_reader->memory.totalRss) << 10;
    m_memoryRss[qSL("text")] = static_cast<quint64>(m_reader->memory.textRss) << 10;
    m_memoryRss[qSL("heap")] = static_cast<quint64>(m_reader->memory.heapRss) << 10;
    m_memoryPss[qSL("total")] = static_cast<quint64>(m_reader->memory.totalPss) << 10;
    m_memoryPss[qSL("text")] = static_cast<quint64>(m_reader->memory.textPss) << 10;
    m_memoryPss[qSL("heap")] = static_cast<quint64>(m_reader->memory.heapPss) << 10;
}

/*!
    \qmlproperty var ProcessStatus::memoryVirtual
    \readonly

    A map of the process's virtual memory usage. For example, the total amount of virtual memory
    is provided through \c memoryVirtual.total. For more information, see the table of
    \l{supported-keys}{supported keys}.

    Calling ProcessStatus::update() updates the value of this property.

    \sa ProcessStatus::update()
*/
QVariantMap ProcessStatus::memoryVirtual() const
{
    return m_memoryVirtual;
}

/*!
    \qmlproperty var ProcessStatus::memoryRss
    \readonly

    A map of the process' Resident Set Size (RSS) memory usage. This is the amount of memory that
    is actually mapped to physical RAM. For more information, see the table of
    \l{supported-keys}{supported keys}.

    Calling ProcessStatus::update() updates the value of this property.

    \sa ProcessStatus::update()
*/
QVariantMap ProcessStatus::memoryRss() const
{
    return m_memoryRss;
}

/*!
    \qmlproperty var ProcessStatus::memoryPss
    \readonly

    A map of the process' Proportional Set Size (PSS) memory usage. This is the proportional share
    of the RSS value in ProcessStatus::memoryRss. For instance, if two processes share 2 MB, then
    the RSS value is 2 MB for each process; the PSS value is 1 MB for each process. As the name
    implies, the code section of shared libraries is generally shared between processes. Memory
    may also be shared by other means provided by the OS, for example, through \c mmap on Linux.
    For more information, see the table of \l{supported-keys}{supported keys}.

    Calling ProcessStatus::update() updates the value of this property.

    \sa ProcessStatus::update()
*/
QVariantMap ProcessStatus::memoryPss() const
{
    return m_memoryPss;
}

/*!
    \qmlproperty bool ProcessStatus::memoryReportingEnabled

    A boolean value that determines whether the memory properties are refreshed each time
    \l{ProcessStatus::update()}{update()} is called. The default value is \c true. In your System
    UI, the process of determining memory consumption adds additional load to the CPU, affecting
    the \c cpuLoad value. If \c cpuLoad needs to be kept accurate, consider disabling memory
    reporting.
*/

bool ProcessStatus::isMemoryReportingEnabled() const
{
    return m_memoryReportingEnabled;
}

void ProcessStatus::setMemoryReportingEnabled(bool enabled)
{
    if (enabled != m_memoryReportingEnabled) {
        m_memoryReportingEnabled = enabled;
        emit memoryReportingEnabledChanged(m_memoryReportingEnabled);
    }
}

/*!
    \qmlproperty list<string> ProcessStatus::roleNames
    \readonly

    Names of the roles that ProcessStatus provides when used as a data source for MonitorModel.

    \sa MonitorModel
*/
QStringList ProcessStatus::roleNames() const
{
    return { qSL("cpuLoad"), qSL("memoryVirtual"), qSL("memoryRss"), qSL("memoryPss") };
}

#include "moc_processstatus.cpp"
