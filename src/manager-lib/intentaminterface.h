// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INTENTAMINTERFACE_H
#define INTENTAMINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtAppManCommon/global.h>

#if QT_CONFIG(am_multi_process)
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
    Q_OBJECT

public:
    IntentClientAMImplementation(IntentServerAMImplementation *serverInterface);

    void initialize(IntentClient *intentClient) noexcept(false) override;
    bool isSystemUI() const override;
    QString currentApplicationId(QObject *hint) override;

    void requestToSystem(const QPointer<IntentClientRequest> &icr) override;
    void replyFromApplication(const QPointer<IntentClientRequest> &icr) override;

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

Q_SIGNALS:
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

#if QT_CONFIG(am_multi_process)

// ... derived for P2P DBus clients
class IntentServerDBusIpcConnection : public IntentServerIpcConnection, public QDBusContext
{
    Q_OBJECT

public:
    static IntentServerDBusIpcConnection *create(const QDBusConnection &connection,
                                                 Application *application,
                                                 IntentServerAMImplementation *iface);
    static IntentServerDBusIpcConnection *find(const QDBusConnection &connection);

    ~IntentServerDBusIpcConnection() override;

    QString requestToSystem(const QString &intentId, const QString &applicationId, const QVariantMap &parameters);
    void replyFromSystem(IntentServerRequest *isr) override;
    void requestToApplication(IntentServerRequest *isr) override;
    void replyFromApplication(const QString &requestId, bool error, const QVariantMap &result);


private:
    IntentServerDBusIpcConnection(const QDBusConnection &connection, Application *application,
                                  IntentServerAMImplementation *iface);

    QString m_connectionName;
    ::IntentInterfaceAdaptor *m_adaptor = nullptr;
};

#endif // QT_CONFIG(am_multi_process)

// server-side IntentHandlers
class IntentServerHandler : public AbstractIntentHandler
{
    Q_OBJECT
    // The following properties cannot be changed after construction (hence, also no real 'changed'
    // signal). These replace the meta-data that's provided through the info.yaml manifests for
    // client-side handlers.
    Q_PROPERTY(QStringList intentIds READ intentIds WRITE setIntentIds NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QVariantMap names READ names WRITE setNames NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QVariantMap descriptions READ descriptions WRITE setDescriptions NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QtAM::Intent::Visibility visibility READ visibility WRITE setVisibility NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QStringList requiredCapabilities READ requiredCapabilities WRITE setRequiredCapabilities NOTIFY dummyChanged FINAL)
    Q_PROPERTY(QVariantMap parameterMatch READ parameterMatch WRITE setParameterMatch NOTIFY dummyChanged FINAL)

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

Q_SIGNALS:
    void intentIdsChanged();
    void requestReceived(QtAM::IntentClientRequest *request);
    void dummyChanged(); // never emitted

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

#endif // INTENTAMINTERFACE_H
