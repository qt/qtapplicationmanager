// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CONTAINERINTERFACE_H
#define CONTAINERINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QtPlugin>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>
#include <QtAppManPluginInterfaces/qtappman_plugininterfaces-config.h>


class ContainerInterface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ContainerInterface)

public:
    // keep these enums in sync with those in amnamespace.h
    enum ExitStatus {
        NormalExit,
        CrashExit,
        ForcedExit
    };
    Q_ENUM(ExitStatus)

    enum RunState {
        NotRunning,
        StartingUp,
        Running,
        ShuttingDown,
    };
    Q_ENUM(RunState)

    enum ProcessError {
        FailedToStart, //### file not found, resource error
        Crashed,
        Timedout,
        ReadError,
        WriteError,
        UnknownError
    };
    Q_ENUM(ProcessError)

    ContainerInterface();
    virtual ~ContainerInterface() override;

    virtual bool attachApplication(const QVariantMap &application) = 0;

    virtual QString controlGroup() const = 0;
    virtual bool setControlGroup(const QString &groupName) = 0;

    virtual bool setProgram(const QString &program) = 0;
    virtual void setBaseDirectory(const QString &baseDirectory) = 0;

    virtual bool isReady() const = 0;

    virtual QString mapContainerPathToHost(const QString &containerPath) const = 0;
    virtual QString mapHostPathToContainer(const QString &hostPath) const = 0;

    virtual bool start(const QStringList &arguments, const QMap<QString, QString> &runtimeEnvironment,
                       const QVariantMap &amConfig) = 0;

    virtual qint64 processId() const = 0;
    virtual RunState state() const = 0;

    virtual void kill() = 0;
    virtual void terminate() = 0;

Q_SIGNALS:
    void ready();
    void started();
    void errorOccured(ContainerInterface::ProcessError processError);
    void finished(int exitCode, ContainerInterface::ExitStatus exitStatus);
    void stateChanged(ContainerInterface::RunState state);
};

// This interface is offered by the AM to the plugin - a pointer to a concrete implementation
// is supplied to the plugin by implementing ContainerManagerInterface::initialize()
class ContainerHelperFunctions
{
    Q_DISABLE_COPY_MOVE(ContainerHelperFunctions)

protected:
    ContainerHelperFunctions() = default;
    ~ContainerHelperFunctions() = default;

public:
    virtual void closeAndClearFileDescriptors(QVector<int> &fdList) = 0;
    virtual QStringList substituteCommand(const QStringList &debugWrapperCommand,
                                          const QString &program, const QStringList &arguments) = 0;
    virtual bool hasRootPrivileges() = 0;

    // this function will run with root privileges and throw std::execptions on error:
    virtual void bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     quint64 namespacePid) = 0;
};

class ContainerManagerInterface
{
    Q_DISABLE_COPY_MOVE(ContainerManagerInterface)

public:
    ContainerManagerInterface();
    virtual ~ContainerManagerInterface();
    virtual bool initialize(ContainerHelperFunctions *helpers);

    virtual QString identifier() const = 0;
    virtual bool supportsQuickLaunch() const = 0;
    virtual void setConfiguration(const QVariantMap &configuration) = 0;

    virtual ContainerInterface *create(bool isQuickLaunch,
                                       const QVector<int> &stdioRedirections,
                                       const QMap<QString, QString> &debugWrapperEnvironment,
                                       const QStringList &debugWrapperCommand) = 0;
};

#define AM_ContainerManagerInterface_iid "io.qt.ApplicationManager.ContainerManagerInterface"

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(ContainerManagerInterface, AM_ContainerManagerInterface_iid)
QT_END_NAMESPACE

#endif // CONTAINERINTERFACE_H
