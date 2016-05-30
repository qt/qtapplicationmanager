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

#include <qglobal.h>

#if defined(QT_DBUS_LIB)

#include <QtPlugin>
#include <QProcess>

#include "abstractruntime.h"
#include "abstractcontainer.h"

#define AM_NATIVE_RUNTIME_AVAILABLE

class Notification;
class NativeRuntimeInterface;
class NativeRuntimeApplicationInterface;

QT_FORWARD_DECLARE_CLASS(QDBusConnection)
QT_FORWARD_DECLARE_CLASS(QDBusServer)


class NativeRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    NativeRuntimeManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();
    bool supportsQuickLaunch() const override;

    AbstractRuntime *create(AbstractContainer *container, const Application *app) override;

    QDBusServer *applicationInterfaceServer() const;

private:
    QDBusServer *m_applicationInterfaceServer;

};

class NativeRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit NativeRuntime(AbstractContainer *container, const Application *app, NativeRuntimeManager *parent);
    ~NativeRuntime();

    bool isQuickLauncher() const override;
    bool attachApplicationToQuickLauncher(const Application *app) override;

    State state() const override;
    qint64 applicationProcessId() const override;
    virtual void openDocument(const QString &document) override;

    bool sendNotificationUpdate(Notification *n);

public slots:
    bool start() override;
    void stop(bool forceKill = false) override;

signals:
    void aboutToStop(); // used for the ApplicationInterface

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onDBusPeerConnection(const QDBusConnection &connection);
    void onLauncherFinishedInitialization();

private:
    bool initialize();
    void shutdown(int exitCode, QProcess::ExitStatus status);

    bool m_isQuickLauncher;
    bool m_needsLauncher;

    QString m_document;
    bool m_shutingDown = false;
    bool m_started = false;
    bool m_launchWhenReady = false;
    bool m_launched = false;
    bool m_dbusConnection = false;
    QString m_dbusConnectionName;

    NativeRuntimeApplicationInterface *m_applicationInterface = 0;
    NativeRuntimeInterface *m_runtimeInterface = 0;
    AbstractContainerProcess *m_process = 0;

    friend class NativeRuntimeManager;
};

#endif //defined(QT_DBUS_LIB)
