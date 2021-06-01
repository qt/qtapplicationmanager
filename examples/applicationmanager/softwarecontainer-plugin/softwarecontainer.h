/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QVariantMap>
#include <QFileInfo>

#include <QtAppManPluginInterfaces/containerinterface.h>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)

class SoftwareContainerManager;

class SoftwareContainer : public ContainerInterface
{
    Q_OBJECT

public:
    SoftwareContainer(SoftwareContainerManager *manager, bool isQuickLaunch, int containerId,
                      int outputFd, const QMap<QString, QString> &debugWrapperEnvironment,
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

    bool start(const QStringList &arguments, const QMap<QString, QString> &runtimeEnvironment,
               const QVariantMap &amConfig) override;

    qint64 processId() const override;
    RunState state() const override;

    void kill() override;
    void terminate() override;

    void containerExited(uint exitCode);

private:
    bool sendCapabilities();
    bool sendBindMounts();

    SoftwareContainerManager *m_manager;
    bool m_isQuickLaunch;
    int m_id;
    QString m_program;
    QString m_baseDir;
    bool m_ready = false;
    qint64 m_pid = 0;
    RunState m_state = NotRunning;
    QVariantMap m_application;
    QString m_appRelativeCodePath;
    QString m_hostPath;
    QString m_containerPath;
    QByteArray m_fifoPath;
    int m_fifoFd = -1;
    int m_outputFd;
    QMap<QString, QString> m_debugWrapperEnvironment;
    QStringList m_debugWrapperCommand;
    QFileInfo m_dbusP2PInfo;
    QFileInfo m_qtPluginPathInfo;
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

    ContainerInterface *create(bool isQuickLaunch,
                               const QVector<int> &stdioRedirections,
                               const QMap<QString, QString> &debugWrapperEnvironment,
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
