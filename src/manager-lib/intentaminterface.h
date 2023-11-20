// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtAppManCommon/global.h>

#if defined(AM_MULTI_PROCESS)
#  include <QtDBus/QDBusConnection>
#  include <QtDBus/QDBusContext>
#endif
#include <QtAppManIntentServer/intentserversysteminterface.h>
#include <QtAppManIntentServer/intent.h>
#include <QtAppManIntentClient/intentclientsysteminterface.h>
#include <QtAppManIntentClient/intenthandler.h>
#include <QtAppManApplication/intentinfo.h>

class IntentInterfaceAdaptor;

QT_BEGIN_NAMESPACE_AM

class Application;
class PackageManager;
class IntentServerRequest;

namespace IntentAMImplementation {
IntentServer *createIntentServerAndClientInstance(PackageManager *packageManager, int disambiguationTimeout,
                                                  int startApplicationTimeout, int replyFromApplicationTimeout,
                                                  int replyFromSystemTimeout);
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
    void requestToApplication(IpcConnection *clientIPC, IntentServerRequest *isr) override;
    void replyFromSystem(IpcConnection *clientIPC, IntentServerRequest *isr) override;

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

    virtual void replyFromSystem(IntentServerRequest *isr) = 0;
    virtual void requestToApplication(IntentServerRequest *isr) = 0;

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

    void replyFromSystem(IntentServerRequest *isr) override;
    void requestToApplication(IntentServerRequest *isr) override;

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
    void replyFromSystem(IntentServerRequest *isr) override;
    void requestToApplication(IntentServerRequest *isr) override;
    void replyFromApplication(const QString &requestId, bool error, const QVariantMap &result);


private:
    IntentServerDBusIpcConnection(QDBusConnection connection, Application *application,
                                  IntentServerAMImplementation *iface);

    QString m_connectionName;
    ::IntentInterfaceAdaptor *m_adaptor = nullptr;
};

#endif // defined(AM_MULTI_PROCESS)

// server-side IntentHandlers
class IntentServerHandler : public AbstractIntentHandler
{
    Q_OBJECT
    // the following properties cannot be changed after construction (hence, also no 'changed' signal)
    // these replace the meta-data that's provided through the info.yaml manifests for client-side
    // handlers
    Q_PROPERTY(QStringList intentIds READ intentIds WRITE setIntentIds FINAL)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon FINAL)
    Q_PROPERTY(QVariantMap names READ names WRITE setNames FINAL)
    Q_PROPERTY(QVariantMap descriptions READ descriptions WRITE setDescriptions FINAL)
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories FINAL)
    Q_PROPERTY(QtAM::Intent::Visibility visibility READ visibility WRITE setVisibility FINAL)
    Q_PROPERTY(QStringList requiredCapabilities READ requiredCapabilities WRITE setRequiredCapabilities FINAL)
    Q_PROPERTY(QVariantMap parameterMatch READ parameterMatch WRITE setParameterMatch FINAL)

public:
    IntentServerHandler(QObject *parent = nullptr);
    ~IntentServerHandler() override;

    QUrl icon() const;
    QVariantMap names() const;
    QVariantMap descriptions() const;
    QStringList categories() const;
    Intent::Visibility visibility() const;
    QStringList requiredCapabilities() const;
    QVariantMap parameterMatch() const;

    void setIntentIds(const QStringList &intentId);
    void setIcon(const QUrl &icon);
    void setNames(const QVariantMap &names);
    void setDescriptions(const QVariantMap &descriptions);
    void setCategories(const QStringList &categories);
    void setVisibility(Intent::Visibility visibility);
    void setRequiredCapabilities(const QStringList &requiredCapabilities);
    void setParameterMatch(const QVariantMap &parameterMatch);

signals:
    void intentIdsChanged();
    void requestReceived(QtAM::IntentClientRequest *request);

protected:
    void classBegin() override;
    void componentComplete() override;
    void internalRequestReceived(IntentClientRequest *request) override;

private:
    Intent *m_intent = nullptr; // DRY: just a container for our otherwise needed members vars
    QVector<Intent *> m_registeredIntents;
    bool m_completed = false;

    Q_DISABLE_COPY_MOVE(IntentServerHandler)
};

QT_END_NAMESPACE_AM
