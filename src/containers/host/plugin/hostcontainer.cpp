/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "hostcontainer.h"
#include "global.h"
#include "executioncontainerfactory.h"
#include <assert.h>
#include "application.h"

static ContainerRegistration<HostContainer> registration(HostContainer::IDENTIFIER, false);

const char *HostContainer::IDENTIFIER = "host";

HostContainer::HostContainer()
{ }

HostContainer::~HostContainer()
{ }

QDir HostContainer::applicationBaseDir()
{
    return m_app->baseDir();
}

bool HostContainer::setApplication(const Application &app)
{
    m_app = &app;
    return true;
}

bool HostContainer::isReady()
{
    return true;
}

ExecutionContainerProcess *HostContainer::startApp(const QStringList &startArguments, const QProcessEnvironment &env)
{
    return startApp(m_app->absoluteCodeFilePath(), startArguments, env);
}

void HostContainer::HostProcess::start(const QString& path, const QStringList& startArguments, const QProcessEnvironment& env)
{
    m_process.setEnvironment(env.toStringList());
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);

    connect(&m_process, &QProcess::started, this, &HostProcess::started, Qt::DirectConnection);
    connect(&m_process, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this, &HostProcess::error, Qt::DirectConnection);
    connect(&m_process, static_cast<void (QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished), this, &HostProcess::finished, Qt::DirectConnection);

    m_process.start(path, startArguments);

    m_pid = m_process.pid();
    m_state = m_process.state();
}

void HostContainer::HostProcess::setWorkingDirectory(const QString &dir)
{
    m_process.setWorkingDirectory(dir);
}

qint64 HostContainer::HostProcess::write(const QByteArray& array) {
    return m_process.write(array);
}

ExecutionContainerProcess *HostContainer::startApp(const QString& app, const QStringList& startArguments, const QProcessEnvironment& env) {
    // Append system environment variables
    QProcessEnvironment completeEnv = QProcessEnvironment::systemEnvironment();
    for (auto& key: env.keys())
        completeEnv.insert(key, env.value(key));

    auto process = new HostProcess();
    process->setWorkingDirectory(m_app->baseDir().absolutePath());
    process->start(app, startArguments, completeEnv);
    return process;
}
