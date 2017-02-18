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

#pragma once

#include <QVariantMap>

#include <QtAppManPluginInterfaces/containerinterface.h>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)

class SoftwareContainerManager;

class SoftwareContainer : public ContainerInterface
{
    Q_OBJECT

public:
    SoftwareContainer(SoftwareContainerManager *manager, int containerId, int outputFd,
                      const QStringList &debugWrapperCommand);
    ~SoftwareContainer();

    SoftwareContainerManager *manager() const;

    bool attachApplication(const QVariantMap &application) override;

    QString controlGroup() const override;
    bool setControlGroup(const QString &groupName) override;

    bool setProgram(const QString &program) override;
    void setBaseDirectory(const QString &baseDirectory) override;

    bool isReady() const override;

    QString mapContainerPathToHost(const QString &containerPath) const override;
    QString mapHostPathToContainer(const QString &hostPath) const override;

    bool start(const QStringList &arguments, const QProcessEnvironment &environment) override;
    bool isStarted() const override;

    qint64 processId() const override;
    QProcess::ProcessState state() const override;

    void kill() override;
    void terminate() override;

    void containerExited(uint exitCode);

private:
    SoftwareContainerManager *m_manager;
    int m_id;
    QString m_program;
    QString m_baseDir;
    bool m_ready = false;
    qint64 m_pid = 0;
    QProcess::ProcessState m_state = QProcess::NotRunning;
    QVariantMap m_application;
    QString m_appRelativeCodePath;
    QString m_hostPath;
    QString m_containerPath;
    QByteArray m_fifoPath;
    int m_fifoFd = -1;
    int m_outputFd;
    QStringList m_debugWrapperCommand;
};

class SoftwareContainerManager : public QObject, public ContainerManagerInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AM_ContainerManagerInterface_iid)
    Q_INTERFACES(ContainerManagerInterface)

public:
    SoftwareContainerManager();

    QString identifier() const override;
    bool supportsQuickLaunch() const override;
    void setConfiguration(const QVariantMap &configuration) override;

    ContainerInterface *create(const QVector<int> &stdioRedirections,
                               const QStringList &debugWrapperCommand) override;
public:
    QDBusInterface *interface() const;
    QVariantMap configuration() const;

private slots:
    void processStateChanged(int containerId, uint processId, bool isRunning, uint exitCode);

private:
    QVariantMap m_configuration;
    QDBusInterface *m_interface = nullptr;
    QMap<int, SoftwareContainer *> m_containers;
};
