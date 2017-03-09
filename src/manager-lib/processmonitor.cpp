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

#include <QDebug>
#include <QCoreApplication>
#include "logging.h"
#include "processmonitor.h"
#include "memorymonitor.h"
#include "application.h"
#include "applicationmanager.h"
#include "abstractruntime.h"

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

QT_BEGIN_NAMESPACE_AM

ProcessMonitor::ProcessMonitor(const QString &appId, QObject *parent)
    : QObject(parent)
    , m_memoryMonitor(nullptr)
    , m_memoryReportingEnabled(false)
    , m_appId(appId)
    , m_pid(0)
{
    obtainPid();
}

ProcessMonitor::~ProcessMonitor()
{
    delete m_memoryMonitor;
}

bool ProcessMonitor::isMemoryReportingEnabled() const
{
    return m_memoryReportingEnabled;
}

void ProcessMonitor::setMemoryReportingEnabled(bool memoryReportingEnabled)
{
    if (m_memoryReportingEnabled == memoryReportingEnabled)
        return;

    if (memoryReportingEnabled) {
        obtainPid();
        if (m_pid == 0) {
            qCWarning(LogSystem) << "WARNING: could not get Pid for app:" << m_appId;
            return;
        }

        if (!m_memoryMonitor) {
            m_memoryMonitor = new MemoryMonitor();
            emit memoryMonitorChanged();
        }

        m_memoryMonitor->setPid(m_pid);
    }

    m_memoryReportingEnabled = memoryReportingEnabled;
    emit memoryReportingEnabledChanged();
}

bool ProcessMonitor::isCpuLoadReportingEnabled() const
{
    return false;
}

void ProcessMonitor::setCpuLoadReportingEnabled(bool cpuReportingEnabled)
{
    Q_UNUSED(cpuReportingEnabled)
}

bool ProcessMonitor::isFpsReportingEnabled() const
{
    return false;
}

void ProcessMonitor::setFpsReportingEnabled(bool fpsReportingEnabled)
{
    Q_UNUSED(fpsReportingEnabled)
}

void ProcessMonitor::readData()
{
    if (m_memoryReportingEnabled) {
        m_memoryMonitor->readData();
    }
    if (m_cpuReportingEnabled) {

    }
    if (m_fpsReportingEnabled) {

    }
}

QAbstractListModel *ProcessMonitor::memoryMonitor()
{
    if (!m_memoryMonitor)
        return nullptr;
    else
        return m_memoryMonitor;

}

QVariant ProcessMonitor::fpsMonitors() const
{
    return QVariant::fromValue(m_fpsMonitors);
}

QString ProcessMonitor::getAppId() const
{
    return m_appId;
}

void ProcessMonitor::obtainPid()
{
    if (m_appId.isEmpty())
        m_pid = QCoreApplication::applicationPid();
    else {
        const Application *app = ApplicationManager::instance()->fromId(m_appId);
        if (app && app->currentRuntime())
            m_pid = app->currentRuntime()->applicationProcessId();
        else
            m_pid = 0;
    }
}

QT_END_NAMESPACE_AM
