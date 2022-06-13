/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QObject>
#include <QtPlugin>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class ContainerInterface : public QObject
{
    Q_OBJECT

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


    virtual ~ContainerInterface();

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

class ContainerManagerInterface
{
public:
    virtual ~ContainerManagerInterface();

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
