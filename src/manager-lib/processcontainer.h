/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include "abstractcontainer.h"

#define AM_HOST_CONTAINER_AVAILABLE

class ProcessContainerManager : public AbstractContainerManager
{
    Q_OBJECT
public:
    ProcessContainerManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();
    bool supportsQuickLaunch() const override;

    AbstractContainer *create() override;
};

class HostProcess : public AbstractContainerProcess
{
    Q_OBJECT

public:
    virtual qint64 processId() const override;
    virtual QProcess::ProcessState state() const override;

public slots:
    void kill() override;
    void terminate() override;

    void start(const QString &program, const QStringList &arguments);
    void setWorkingDirectory(const QString &dir) override;
    void setProcessEnvironment(const QProcessEnvironment &environment) override;
    void setStopBeforeExec(bool stopBeforeExec);

private:
    class MyQProcess : public QProcess
    {
    protected:
        void setupChildProcess() override;
    public:
        bool m_stopBeforeExec = false;
    };

    MyQProcess m_process;
};

class ProcessContainer : public AbstractContainer
{
    Q_OBJECT

public:
    explicit ProcessContainer(ProcessContainerManager *manager);
    ~ProcessContainer();

    bool isReady() override;

    AbstractContainerProcess *start(const QStringList &arguments, const QProcessEnvironment &environment) override;
};
