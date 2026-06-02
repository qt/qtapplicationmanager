// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef BUBBLEWRAPCONTAINER_H
#define BUBBLEWRAPCONTAINER_H

#include <QVariantMap>
#include <QFileInfo>
#include <QProcess>

#include <QtAppManPluginInterfaces/containerinterface.h>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)

class BubblewrapContainerManager;

class BubblewrapContainer : public ContainerInterface
{
    Q_OBJECT

public:
    BubblewrapContainer(BubblewrapContainerManager *manager,
                      const QVector<int> &stdioRedirections,
                      const QMap<QString, QString> &debugWrapperEnvironment,
                      const QStringList &debugWrapperCommand);
    ~BubblewrapContainer() override;

    BubblewrapContainerManager *manager() const;

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
    int processFd() const override;
    RunState state() const override;

    void stop(ContainerInterface::ExitStatus exitStatus) override;

private:
    void setupCustomBindMounts(bool ignoreCapabilities = false);
    void containerExited(int exitCode, QProcess::ExitStatus exitStatus);

    enum class NetworkScriptEvent {
        Start,
        Stop,
        QuickLaunch,
    };
    bool runNetworkSetupScript(NetworkScriptEvent event);

private:
    BubblewrapContainerManager *m_manager;
    QString m_program;
    QString m_baseDir;
    bool m_ready = false;
    qint64 m_pid = 0;
    int m_pidfd = -1;
    quint64 m_namespacePid = 0;
    quint64 m_namespacePidInode = 0;  // pidfs inode of m_namespacePid, captured at child-pid receipt
    RunState m_state = NotRunning;
    QVariantMap m_application;
    QString m_appRelativeCodePath;
    QString m_hostPath;
    QString m_containerPath;
    std::array<int, 2> m_statusPipeFd = { -1, -1 };
    QVector<int> m_stdioRedirections;
    QMap<QString, QString> m_debugWrapperEnvironment;
    QStringList m_debugWrapperCommand;
    QFileInfo m_dbusP2PInfo;
    QByteArray m_statusBuffer;
    bool m_hasExitCode = false;
    int m_exitCode = 0;
    QProcess::ExitStatus m_exitStatus = { };
    QString m_currentControlGroup;
    static bool s_hasCGroupV2;
    QMap<QString, QString> m_roBindMounts;
    QMap<QString, QString> m_rwBindMounts;

    QProcess *m_process = nullptr;
};

class BubblewrapContainerManager : public QObject, public ContainerManagerInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AM_ContainerManagerInterface_iid)
    Q_INTERFACES(ContainerManagerInterface)

public:
    BubblewrapContainerManager();

    bool initialize(ContainerHelperFunctions *helpers) override;
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
    ContainerHelperFunctions *helpers() const;
    QString bwrapPath() const;
    QStringList bwrapArguments() const;
    QString networkSetupScript() const;

private:
    ContainerHelperFunctions *m_helpers = nullptr;
    QVariantMap m_configuration;
    QStringList m_bwrapArguments;
    QString m_bwrapPath;
    QString m_networkSetupScript;
};

#endif // BUBBLEWRAPCONTAINER_H
