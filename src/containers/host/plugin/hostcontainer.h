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

#include "executioncontainer.h"

class NativeProcess;

class HostContainer : public ExecutionContainer
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.pelagicore.applicationmanager.container.host")
    Q_INTERFACES (ExecutionContainer)

    class HostProcess : public ExecutionContainerProcess
    {
    public:
        void start(const QString& path, const QStringList& startArguments, const QProcessEnvironment& env);
        void setWorkingDirectory(const QString &dir);

        qint64 write(const QByteArray& array) override;

    private:
        QProcess m_process;
    };

public:

    static const char* IDENTIFIER;

    HostContainer();

    ~HostContainer();

    QDir applicationBaseDir() override;

    bool setApplication(const Application& app) override;

    bool isReady() override;

    ExecutionContainerProcess *startApp(const QStringList& startArguments, const QProcessEnvironment& env) override;

    ExecutionContainerProcess *startApp(const QString& app, const QStringList& startArguments, const QProcessEnvironment& env) override;

private:
    const Application* m_app = nullptr;

};
