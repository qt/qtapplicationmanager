/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QProcess>

#include "global.h"

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

class Application;
class ExecutionContainer;

class AM_EXPORT AbstractRuntime : public QObject
{
    Q_OBJECT
public:
    virtual ~AbstractRuntime();

    enum State {
        Inactive,
        Startup,
        Active,
        Shutdown
    };

    static const Q_PID INVALID_PID;

    static QString identifier();

    virtual bool inProcess() const;

    virtual bool create(const Application *app) = 0;
    const Application *application() const;

    virtual State state() const = 0;

    /**
     * Returns the PID of the application
     */
    virtual Q_PID applicationPID() const = 0;

    enum { SecurityTokenSize = 16 };
    QByteArray securityToken() const;

    virtual void openDocument(const QString &document);

    void setInProcessQmlEngine(QQmlEngine *view);
    QQmlEngine* inProcessQmlEngine() const;

    void setExecutionContainer(ExecutionContainer* container) { m_container = container; }
    ExecutionContainer* executionContainer() { return m_container; }

    static void setConfiguration(const QVariantMap &config);

public slots:
    virtual bool start() = 0;
    virtual void stop(bool forceKill = false) = 0;


signals:
    void finished(int exitCode, QProcess::ExitStatus status);

#if !defined(AM_HEADLESS)
    // these signals are for in-process mode runtimes only
    void inProcessSurfaceItemReady(QQuickItem *window);
    void inProcessSurfaceItemClosing(QQuickItem *window);
    void inProcessSurfaceItemFullscreenChanging(QQuickItem *window, bool fullscreen);
#endif

protected:
    explicit AbstractRuntime(QObject *parent = 0);

    QVariantMap configuration() const;

    const Application *m_app;
    ExecutionContainer *m_container = 0;
    QByteArray m_securityToken;
    QQmlEngine *m_inProcessQmlEngine = 0;

private:
    static QVariantMap s_config;
};
