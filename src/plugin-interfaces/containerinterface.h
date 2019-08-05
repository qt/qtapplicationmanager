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

#include <QObject>
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
    void errorOccured(ProcessError processError);
    void finished(int exitCode, ExitStatus exitStatus);
    void stateChanged(RunState state);
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
