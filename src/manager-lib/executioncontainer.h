/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QDir>

#include "global.h"

#include <memory>

class Application;

class AM_EXPORT ExecutionContainerProcess : public QObject
{
    Q_OBJECT

public:
    QProcess::ProcessState state();

    void kill();
    void terminate();

    Q_PID pid();

    /**
     * Write bytes to the stdin of the process
     */
    virtual qint64 write(const QByteArray& array) = 0;

    Q_SIGNAL void started();
    Q_SIGNAL void error(QProcess::ProcessError error);
    Q_SIGNAL void finished(int returnCode, QProcess::ExitStatus status);

protected:
    void setFinished(int returnCode, QProcess::ExitStatus status);

    Q_PID m_pid;
    QProcess::ProcessState m_state = QProcess::ProcessState::NotRunning;
};

class AM_EXPORT ExecutionContainer : public QObject
{
    Q_OBJECT
public:
    virtual ~ExecutionContainer();

    virtual bool init();

    virtual QDir applicationBaseDir() = 0;

    virtual bool setApplication(const Application& app) = 0;

    virtual bool isReady() = 0;

    virtual ExecutionContainerProcess *startApp(const QStringList& startArguments, const QProcessEnvironment& env) = 0;

    virtual ExecutionContainerProcess *startApp(const QString& app, const QStringList& startArguments, const QProcessEnvironment& env) = 0;

    Q_SIGNAL void ready();

};

Q_DECLARE_INTERFACE(ExecutionContainer, "com.pelagicore.applicationmanager.Container")

