/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QtAppManManager/abstractcontainer.h>

#define AM_HOST_CONTAINER_AVAILABLE

QT_BEGIN_NAMESPACE_AM

class ProcessContainerManager : public AbstractContainerManager
{
    Q_OBJECT
public:
    explicit ProcessContainerManager(QObject *parent = 0);
    explicit ProcessContainerManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();
    bool supportsQuickLaunch() const override;

    AbstractContainer *create() override;
    AbstractContainer *create(const ContainerDebugWrapper &debugWrapper) override;
};

class HostProcess : public AbstractContainerProcess
{
    Q_OBJECT

public:
    HostProcess();

    virtual qint64 processId() const override;
    virtual QProcess::ProcessState state() const override;

    void setRedirections(const QVector<int> &stdRedirections);

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
        QVector<int> m_stdRedirections;
    };

    MyQProcess m_process;
};

class ProcessContainer : public AbstractContainer
{
    Q_OBJECT

public:
    explicit ProcessContainer(ProcessContainerManager *manager);
    explicit ProcessContainer(const ContainerDebugWrapper &debugWrapper, ProcessContainerManager *manager);
    ~ProcessContainer();

    QString controlGroup() const override;
    bool setControlGroup(const QString &groupName) override;

    bool isReady() override;

    AbstractContainerProcess *start(const QStringList &arguments, const QProcessEnvironment &environment) override;

private:
    QString m_currentControlGroup;
    bool m_useDebugWrapper = false;
    ContainerDebugWrapper m_debugWrapper;
};

QT_END_NAMESPACE_AM
