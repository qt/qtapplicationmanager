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
class AbstractContainer;
class AbstractRuntime;
class AbstractContainer;

class AbstractRuntimeManager : public QObject
{
    Q_OBJECT

public:
    AbstractRuntimeManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();

    QString identifier() const;
    virtual bool inProcess() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractRuntime *create(AbstractContainer *container, const Application *app) = 0;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

private:
    QString m_id;
    QVariantMap m_configuration;
};


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

    AbstractContainer *container() const;
    const Application *application() const;
    AbstractRuntimeManager *manager() const;

    virtual bool isQuickLauncher() const;
    virtual bool attachApplicationToQuickLauncher(const Application *app);

    virtual State state() const = 0;

    enum { SecurityTokenSize = 16 };
    QByteArray securityToken() const;

    virtual void openDocument(const QString &document);

    void setInProcessQmlEngine(QQmlEngine *view);
    QQmlEngine* inProcessQmlEngine() const;

    virtual qint64 applicationProcessId() const = 0;

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
    explicit AbstractRuntime(AbstractContainer *container, const Application *app, AbstractRuntimeManager *manager);

    QVariantMap configuration() const;

    AbstractContainer *m_container;
    const Application *m_app;
    AbstractRuntimeManager *m_manager;

    QByteArray m_securityToken;
    QQmlEngine *m_inProcessQmlEngine = nullptr;

    friend class AbstractRuntimeManager;
};
