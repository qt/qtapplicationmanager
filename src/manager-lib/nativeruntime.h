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

#include <qglobal.h>

#if defined(QT_DBUS_LIB)

#include <QtPlugin>
#include <QProcess>

#include "abstractruntime.h"
#include "executioncontainer.h"

#define AM_NATIVE_RUNTIME_AVAILABLE

class Notification;
class NativeRuntimeInterface;
class NativeRuntimeApplicationInterface;

QT_FORWARD_DECLARE_CLASS(QDBusConnection)
QT_FORWARD_DECLARE_CLASS(QDBusServer)

class NativeRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit NativeRuntime(QObject *parent = 0);

    static QString identifier();

    bool create(const Application *app);

    State state() const;

    Q_PID applicationPID() const override;

    static void createInProcess(bool inProcess);

    virtual void openDocument(const QString &document);

    bool sendNotificationUpdate(Notification *n);

public slots:
    bool start();
    void stop(bool forceKill = false);

signals:
    void aboutToStop(); // used for the ApplicationInterface

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onDBusPeerConnection(const QDBusConnection &connection);
    void onLauncherFinishedInitialization();

private:
    QString m_launcherCommand;

    QString m_document;
    bool m_shutingDown = false;

    bool m_launchWhenReady = false;
    bool m_launched = false;
    bool m_dbusConnection = false;

    NativeRuntimeApplicationInterface *m_applicationInterface = 0;
    NativeRuntimeInterface *m_runtimeInterface = 0;
    ExecutionContainerProcess *m_process = 0;

    static QDBusServer *s_applicationInterfaceServer;
};

#endif //defined(QT_DBUS_LIB)
