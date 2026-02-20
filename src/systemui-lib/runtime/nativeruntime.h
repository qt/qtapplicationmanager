// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef NATIVERUNTIME_H
#define NATIVERUNTIME_H

#include <QtCore/qglobal.h>

#include <QtCore/QtPlugin>
#include <QtCore/QVector>

#include <QtAppManSystemUI/abstractruntime.h>
#include <QtAppManSystemUI/abstractcontainer.h>
#include <QtAppManSystemUI/amnamespace.h>
#include <QtAppManSystemUI/dbuscontextadaptor.h>


QT_FORWARD_DECLARE_CLASS(QDBusConnection)
QT_FORWARD_DECLARE_CLASS(QDBusServer)

class RuntimeInterfaceAdaptor;
class NotificationInterfaceAdaptor;
class ApplicationInterfaceAdaptor;

QT_BEGIN_NAMESPACE_AM

class Notification;
class NativeRuntime;


class Q_APPMANSYSTEMUI_EXPORT NativeRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit NativeRuntimeManager(QObject *parent = nullptr);
    explicit NativeRuntimeManager(const QString &id, QObject *parent = nullptr);

    bool supportsQuickLaunch() const override;

    AbstractRuntime *create(AbstractContainer *container, Application *app) override;
};

class Q_APPMANSYSTEMUI_EXPORT NativeRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    ~NativeRuntime() override;

    bool isQuickLauncher() const override;
    bool attachApplicationToQuickLauncher(Application *app) override;

    qint64 applicationProcessId() const override;
    void openDocument(const QString &document, const QString &mimeType) override;
    void setSlowAnimations(bool slow) override;

    bool sendNotificationUpdate(Notification *n);

    void applicationFinishedInitialization(); // called by the D-Bus adaptor

    void setApplicationExtraDirs(const QMap<QString, QString> &extraPaths) override;

    bool start() override;
    void stop(Am::ExitStatus exitStatus) override;

Q_SIGNALS:
    void aboutToStop(); // used for the ApplicationInterface

    void applicationConnectedToPeerDBus(const QDBusConnection &connection,
                                        QtAM::Application *application);
    void applicationReadyOnPeerDBus(const QDBusConnection &connection,
                                    QtAM::Application *application);
    void applicationDisconnectedFromPeerDBus(const QDBusConnection &connection,
                                             QtAM::Application *application);

private:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QtAM::Am::ExitStatus status);
    void onProcessError(QtAM::Am::ProcessError error);
    void onDBusPeerConnection(const QDBusConnection &connection);

protected:
    explicit NativeRuntime(AbstractContainer *container, Application *app, NativeRuntimeManager *parent);

private:
    bool initialize();
    void shutdown(int exitCode, Am::ExitStatus status);
    QDBusServer *applicationInterfaceServer() const;
    bool startApplicationViaLauncher();

    bool m_isQuickLauncher;
    bool m_startedViaLauncher;

    QString m_document;
    QString m_mimeType;
    bool m_connectedToApplicationInterface = false;
    bool m_dbusConnection = false;
    QString m_dbusConnectionName;
    QMap<QString, QString> m_applicationExtraDirs;

    std::unique_ptr<DBusContextAdaptor> m_dbusApplicationInterface;
    std::unique_ptr<DBusContextAdaptor> m_dbusRuntimeInterface;
    std::unique_ptr<DBusContextAdaptor> m_dbusNotificationInterface;
    AbstractContainerProcess *m_process = nullptr;
    QDBusServer *m_applicationInterfaceServer;
    bool m_slowAnimations = false;
    QVariantMap m_openGLConfiguration;

    friend class NativeRuntimeManager;
};

QT_END_NAMESPACE_AM

#endif // NATIVERUNTIME_H
