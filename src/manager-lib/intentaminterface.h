/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
#include <QVector>
#include <QString>
#include <QVariantMap>
#include <QList>
#if defined(AM_MULTI_PROCESS)
#  include <QDBusConnection>
#  include <QDBusContext>
#endif
#include <QtAppManCommon/global.h>
#include <QtAppManIntentServer/intentserversysteminterface.h>
#include <QtAppManIntentClient/intentclientsysteminterface.h>
#include <QtAppManApplication/intentinfo.h>

class IntentInterfaceAdaptor;

QT_BEGIN_NAMESPACE_AM

class Application;
class PackageManager;
class IntentServerRequest;

namespace IntentAMImplementation {
IntentServer *createIntentServerAndClientInstance(PackageManager *packageManager, const QMap<QString, int> &timeouts);
}

// the server side
class IntentServerAMImplementation : public IntentServerSystemInterface
{
    Q_OBJECT

public:
    void setIntentClientSystemInterface(IntentClientSystemInterface *iface);
    IntentClientSystemInterface *intentClientSystemInterface() const;

    void initialize(IntentServer *intentServer) override;

    bool checkApplicationCapabilities(const QString &applicationId,
                                      const QStringList &requiredCapabilities) override;

    IpcConnection *findClientIpc(const QString &appId) override;

    void startApplication(const QString &appId) override;
    void requestToApplication(IpcConnection *clientIPC, IntentServerRequest *irs) override;
    void replyFromSystem(IpcConnection *clientIPC, IntentServerRequest *irs) override;

private:
    IntentClientSystemInterface *m_icsi = nullptr;
};

// the in-process client side
class IntentClientAMImplementation : public IntentClientSystemInterface
{
public:
    IntentClientAMImplementation(IntentServerAMImplementation *serverInterface);

    void initialize(IntentClient *intentClient) Q_DECL_NOEXCEPT_EXPR(false) override;
    QString currentApplicationId(QObject *hint) override;

    void requestToSystem(QPointer<IntentClientRequest> icr) override;
    void replyFromApplication(QPointer<IntentClientRequest> icr) override;

private:
    IntentServerSystemInterface *m_issi;
};

// each instance represents one server->client connection
class IntentServerIpcConnection : public QObject
{
    Q_OBJECT

public:
    ~IntentServerIpcConnection() override;

    static IntentServerIpcConnection *find(const QString &appId);

    Application *application() const;
    virtual QString applicationId() const;
    bool isInProcess() const;

    bool isReady() const;
    void setReady(Application *application);

    virtual void replyFromSystem(IntentServerRequest *irs) = 0;
    virtual void requestToApplication(IntentServerRequest *irs) = 0;

signals:
    void applicationIsReady(const QString &applicationId);

protected:
    IntentServerIpcConnection(bool inProcess, Application *application, IntentServerAMImplementation *iface);

    Application *m_application;
    IntentServerAMImplementation *m_interface;
    bool m_inprocess = true;
    bool m_ready = false;

    static QList<IntentServerIpcConnection *> s_ipcConnections;
};

// ... derived for in-process clients
class IntentServerInProcessIpcConnection : public IntentServerIpcConnection
{
    Q_OBJECT

public:
    static IntentServerInProcessIpcConnection *create(Application *application, IntentServerAMImplementation *iface);
    static IntentServerInProcessIpcConnection *createSystemUi(IntentServerAMImplementation *iface);

    QString applicationId() const override;

    void replyFromSystem(IntentServerRequest *irs) override;
    void requestToApplication(IntentServerRequest *irs) override;

private:
    IntentServerInProcessIpcConnection(Application *application, IntentServerAMImplementation *iface);

    bool m_isSystemUi = false;
};

#if defined(AM_MULTI_PROCESS)

// ... derived for P2P DBus clients
class IntentServerDBusIpcConnection : public IntentServerIpcConnection, public QDBusContext
{
    Q_OBJECT

public:
    static IntentServerDBusIpcConnection *create(QDBusConnection connection, Application *application,
                                                 IntentServerAMImplementation *iface);
    static IntentServerDBusIpcConnection *find(QDBusConnection connection);

    ~IntentServerDBusIpcConnection() override;

    QString requestToSystem(const QString &intentId, const QString &applicationId, const QVariantMap &parameters);
    void replyFromSystem(IntentServerRequest *irs) override;
    void requestToApplication(IntentServerRequest *irs) override;
    void replyFromApplication(const QString &requestId, bool error, const QVariantMap &result);


private:
    IntentServerDBusIpcConnection(QDBusConnection connection, Application *application,
                                  IntentServerAMImplementation *iface);

    QString m_connectionName;
    ::IntentInterfaceAdaptor *m_adaptor = nullptr;
};

#endif // defined(AM_MULTI_PROCESS)

QT_END_NAMESPACE_AM
