// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef ABSTRACTRUNTIME_H
#define ABSTRACTRUNTIME_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantMap>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>
#include <QtAppManSystemUI/amnamespace.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractContainer;
class AbstractRuntime;
class AbstractContainer;
class InProcessSurfaceItem;

class Q_APPMANSYSTEMUI_EXPORT AbstractRuntimeManager : public QObject
{
    Q_OBJECT

public:
    AbstractRuntimeManager(const QString &id, QObject *parent = nullptr);

    QString identifier() const;
    virtual bool inProcess() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractRuntime *create(AbstractContainer *container, Application *app) = 0;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

    static QList<QtAM::AbstractRuntime *> fromProcessId(qint64 pid, int pidfd = -1);

private:
    QString m_id;
    QVariantMap m_configuration;

    friend class AbstractRuntime;
    static QList<AbstractRuntime *> s_allRuntimes;
};

class Q_APPMANSYSTEMUI_EXPORT RuntimeSignaler : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void aboutToStart(QtAM::AbstractRuntime *runtime);
    // this signal is for in-process mode runtimes only
    void inProcessSurfaceItemReady(QtAM::AbstractRuntime *runtime,
                                   QSharedPointer<QtAM::InProcessSurfaceItem> window);

};

class Q_APPMANSYSTEMUI_EXPORT AbstractRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QtAM::AbstractContainer *container READ container CONSTANT FINAL)
    Q_PROPERTY(QString runtimeId READ runtimeId CONSTANT FINAL REVISION(6, 10))

public:
    ~AbstractRuntime() override;

    AbstractContainer *container() const;
    Application *application() const;
    AbstractRuntimeManager *manager() const;

    QString runtimeId() const;

    virtual bool isQuickLauncher() const;
    virtual bool attachApplicationToQuickLauncher(Application *app);

    Am::RunState state() const;

    enum { SecurityTokenSize = 32 };
    QByteArray securityToken() const;

    virtual void openDocument(const QString &document, const QString &mimeType);

    virtual void setSlowAnimations(bool slow);

    virtual void setApplicationExtraDirs(const QMap<QString, QString> &extraPaths);

    void setInProcessQmlEngine(QQmlEngine *view);
    QQmlEngine* inProcessQmlEngine() const;

    virtual qint64 applicationProcessId() const = 0;

    QVariantMap systemProperties() const;

    virtual bool start() = 0;
    virtual void stop(Am::ExitStatus exitStatus) = 0;

    static RuntimeSignaler* signaler();

Q_SIGNALS:
    void stateChanged(QtAM::Am::RunState newState);
    void finished(int exitCode, QtAM::Am::ExitStatus status);

protected:
    explicit AbstractRuntime(AbstractContainer *container, Application *app, AbstractRuntimeManager *manager);
    void setState(Am::RunState newState);

    QVariantMap configuration() const;

    AbstractContainer *m_container;
    QPointer<Application> m_app;
    AbstractRuntimeManager *m_manager;

    QByteArray m_securityToken;
    QQmlEngine *m_inProcessQmlEngine = nullptr;
    Am::RunState m_state = Am::NotRunning;

    friend class AbstractRuntimeManager;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QtAM::AbstractRuntime *)

#endif // ABSTRACTRUNTIME_H
