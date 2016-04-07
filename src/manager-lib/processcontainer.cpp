/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "global.h"
#include "containerfactory.h"
#include "application.h"
#include "processcontainer.h"

#if defined(Q_OS_UNIX)
#  include <csignal>
#  include <unistd.h>
#endif


void HostProcess::start(const QString &program, const QStringList &arguments)
{
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);

    connect(&m_process, &QProcess::started, this, &HostProcess::started);
    connect(&m_process, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
            this, &HostProcess::error);
    connect(&m_process, static_cast<void (QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),
            this, &HostProcess::finished);
    connect(&m_process, &QProcess::stateChanged, this, &HostProcess::stateChanged);

    m_process.start(program, arguments);
}

void HostProcess::setWorkingDirectory(const QString &dir)
{
    m_process.setWorkingDirectory(dir);
}

void HostProcess::setProcessEnvironment(const QProcessEnvironment &environment)
{
    m_process.setProcessEnvironment(environment);
}

void HostProcess::kill()
{
    m_process.kill();
}

void HostProcess::terminate()
{
    m_process.terminate();
}

qint64 HostProcess::processId() const
{
    return m_process.processId();
}

QProcess::ProcessState HostProcess::state() const
{
    return m_process.state();
}



ProcessContainer::ProcessContainer(ProcessContainerManager *manager)
    : AbstractContainer(manager)
{ }

ProcessContainer::~ProcessContainer()
{ }

bool ProcessContainer::isReady()
{
    return true;
}

AbstractContainerProcess *ProcessContainer::start(const QStringList &arguments, const QProcessEnvironment &environment)
{
    if (!QFile::exists(m_program))
        return nullptr;

    QProcessEnvironment completeEnv = QProcessEnvironment::systemEnvironment();
    completeEnv.insert(environment);

    HostProcess *process = new HostProcess();
    process->setWorkingDirectory(m_baseDirectory);
    process->setProcessEnvironment(completeEnv);
    process->setStopBeforeExec(configuration().value(qSL("stopBeforeExec")).toBool());
    process->start(m_program, arguments);
    return process;
}

ProcessContainerManager::ProcessContainerManager(const QString &id, QObject *parent)
    : AbstractContainerManager(id, parent)
{ }

QString ProcessContainerManager::defaultIdentifier()
{
    return qSL("process");
}

bool ProcessContainerManager::supportsQuickLaunch() const
{
    return true;
}

AbstractContainer *ProcessContainerManager::create()
{
    return new ProcessContainer(this);
}

void HostProcess::setStopBeforeExec(bool stopBeforeExec)
{
    m_process.m_stopBeforeExec = stopBeforeExec;
}

void HostProcess::MyQProcess::setupChildProcess()
{
    if (m_stopBeforeExec) {
#if defined(Q_OS_UNIX)
        fprintf(stderr, "\n*** a 'process' container was started in stopped state ***\nthe process is suspended via SIGSTOP and you can attach a debugger to it via\n\n   gdb -p %d\n\n", getpid());
        raise(SIGSTOP);
#endif
    }
}
