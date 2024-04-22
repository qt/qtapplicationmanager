// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef SOFTWARECONTAINER_H
#define SOFTWARECONTAINER_H

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

private Q_SLOTS:
    void processStateChanged(int containerId, uint processId, bool isRunning, uint exitCode);

private:
    QVariantMap m_configuration;
    QDBusInterface *m_interface = nullptr;
    QMap<int, SoftwareContainer *> m_containers;
};

#endif // SOFTWARECONTAINER_H
