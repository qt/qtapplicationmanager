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

#include <QObject>
#include <QString>
#include <QProcess>
#include <QDir>

#include "global.h"

class Application;
class AbstractContainer;

class AbstractContainerManager : public QObject
{
    Q_OBJECT
public:
    AbstractContainerManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();

    QString identifier() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractContainer *create() = 0;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

private:
    QString m_id;
    QVariantMap m_configuration;
};

class AbstractContainerProcess : public QObject
{
    Q_OBJECT

public:
    virtual qint64 processId() const = 0;
    virtual QProcess::ProcessState state() const = 0;
    virtual void setWorkingDirectory(const QString &dir) = 0;
    virtual void setProcessEnvironment(const QProcessEnvironment &environment) = 0;

public slots:
    virtual void kill() = 0;
    virtual void terminate() = 0;

signals:
    void started();
    void error(QProcess::ProcessError error);
    void finished(int exitCode, QProcess::ExitStatus status);
    void stateChanged(QProcess::ProcessState newState);
};

class AbstractContainer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString controlGroup READ controlGroup WRITE setControlGroup)

public:
    virtual ~AbstractContainer();

    virtual QString controlGroup() const;
    virtual bool setControlGroup(const QString &groupName);

    bool setProgram(const QString &program);
    void setBaseDirectory(const QString &baseDirectory);

    virtual bool isReady() = 0;

    virtual QString mapContainerPathToHost(const QString &containerPath) const;
    virtual QString mapHostPathToContainer(const QString &hostPath) const;

    virtual AbstractContainerProcess *start(const QStringList &arguments, const QProcessEnvironment &env) = 0;

    AbstractContainerProcess *process() const;

signals:
    void ready();

protected:
    explicit AbstractContainer(AbstractContainerManager *manager);

    QVariantMap configuration() const;

    QString m_program;
    QString m_baseDirectory;
    AbstractContainerManager *m_manager;
    AbstractContainerProcess *m_process = nullptr;
};

Q_DECLARE_METATYPE(AbstractContainer *)
