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

#include <functional>

#include <QProcess>

class ContainerInterface : public QObject
{
    Q_OBJECT

public:
    virtual ~ContainerInterface();

    virtual bool attachApplication(const QVariantMap &application) = 0;

    virtual QString controlGroup() const = 0;
    virtual bool setControlGroup(const QString &groupName) = 0;

    virtual bool setProgram(const QString &program) = 0;
    virtual void setBaseDirectory(const QString &baseDirectory) = 0;

    virtual bool isReady() const = 0;

    virtual QString mapContainerPathToHost(const QString &containerPath) const = 0;
    virtual QString mapHostPathToContainer(const QString &hostPath) const = 0;

    virtual bool start(const QStringList &arguments, const QProcessEnvironment &env) = 0;
    virtual bool isStarted() const = 0;

    virtual qint64 processId() const = 0;
    virtual QProcess::ProcessState state() const = 0;

    virtual void kill() = 0;
    virtual void terminate() = 0;

Q_SIGNALS:
    void ready();
    void started();
    void errorOccured(QProcess::ProcessError processError);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void stateChanged(QProcess::ProcessState state);
};

class ContainerManagerInterface
{
public:
    virtual ~ContainerManagerInterface();

    virtual QString identifier() const = 0;
    virtual bool supportsQuickLaunch() const = 0;
    virtual void setConfiguration(const QVariantMap &configuration) = 0;

    virtual ContainerInterface *create() = 0;
};

#define AM_ContainerManagerInterface_iid "io.qt.ApplicationManager.ContainerManagerInterface"

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(ContainerManagerInterface, AM_ContainerManagerInterface_iid)
QT_END_NAMESPACE
