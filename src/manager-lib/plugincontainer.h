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

#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManPluginInterfaces/containerinterface.h>

QT_BEGIN_NAMESPACE_AM

class PluginContainerManager : public AbstractContainerManager
{
    Q_OBJECT
public:
    PluginContainerManager(ContainerManagerInterface *managerInterface, QObject *parent = nullptr);

    static QString defaultIdentifier();
    bool supportsQuickLaunch() const override;

    AbstractContainer *create(Application *app, const QVector<int> &stdioRedirections,
                              const QMap<QString, QString> &debugWrapperEnvironment,
                              const QStringList &debugWrapperCommand) override;

    void setConfiguration(const QVariantMap &configuration) override;

private:
    ContainerManagerInterface *m_interface;
};

class PluginContainer;

class PluginContainerProcess : public AbstractContainerProcess
{
    Q_OBJECT

public:
    PluginContainerProcess(PluginContainer *container);

    qint64 processId() const override;
    Am::RunState state() const override;

public slots:
    void kill() override;
    void terminate() override;

private:
    PluginContainer *m_container;
};

class PluginContainer : public AbstractContainer
{
    Q_OBJECT

public:
    virtual ~PluginContainer() override;

    QString controlGroup() const override;
    bool setControlGroup(const QString &groupName) override;

    bool setProgram(const QString &program) override;
    void setBaseDirectory(const QString &baseDirectory) override;

    bool isReady() override;

    QString mapContainerPathToHost(const QString &containerPath) const override;
    QString mapHostPathToContainer(const QString &hostPath) const override;

    AbstractContainerProcess *start(const QStringList &arguments, const QMap<QString, QString> &env,
                                    const QVariantMap &amConfig) override;

protected:
    explicit PluginContainer(AbstractContainerManager *manager, Application *app, ContainerInterface *containerInterface);
    ContainerInterface *m_interface;
    PluginContainerProcess *m_process;
    bool m_startCalled = false;

    friend class PluginContainerProcess;
    friend class PluginContainerManager;
};

QT_END_NAMESPACE_AM
