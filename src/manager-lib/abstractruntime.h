// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantMap>

#include <QtAppManCommon/global.h>
#include <QtAppManManager/amnamespace.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractContainer;
class AbstractRuntime;
class AbstractContainer;
class InProcessSurfaceItem;

class AbstractRuntimeManager : public QObject
{
    Q_OBJECT

public:
    AbstractRuntimeManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();

    QString identifier() const;
    virtual bool inProcess() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractRuntime *create(AbstractContainer *container, Application *app) = 0;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

    QVariantMap systemPropertiesBuiltIn() const;
    QVariantMap systemProperties3rdParty() const;
    void setSystemProperties(const QVariantMap &thirdParty, const QVariantMap &builtIn);

    QVariantMap systemOpenGLConfiguration() const;
    void setSystemOpenGLConfiguration(const QVariantMap &openGLConfiguration);

    QStringList iconThemeSearchPaths() const;
    QString iconThemeName() const;
    void setIconTheme(const QStringList &themeSearchPaths, const QString &themeName);

private:
    QString m_id;
    QVariantMap m_configuration;
    QVariantMap m_systemPropertiesBuiltIn;
    QVariantMap m_systemProperties3rdParty;
    QVariantMap m_systemOpenGLConfiguration;
    QString m_iconThemeName;
    QStringList m_iconThemeSearchPaths;
};

class RuntimeSignaler : public QObject
{
    Q_OBJECT
signals:
    void aboutToStart(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime);
    // this signal is for in-process mode runtimes only
    void inProcessSurfaceItemReady(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime,
                                   QSharedPointer<QT_PREPEND_NAMESPACE_AM(InProcessSurfaceItem)> window);

};

class AbstractRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AbstractContainer *container READ container CONSTANT FINAL)

public:
    virtual ~AbstractRuntime();

    AbstractContainer *container() const;
    Application *application() const;
    AbstractRuntimeManager *manager() const;

    virtual bool isQuickLauncher() const;
    virtual bool attachApplicationToQuickLauncher(Application *app);

    Am::RunState state() const;

    enum { SecurityTokenSize = 16 };
    QByteArray securityToken() const;

    virtual void openDocument(const QString &document, const QString &mimeType);

    virtual void setSlowAnimations(bool slow);

    void setInProcessQmlEngine(QQmlEngine *view);
    QQmlEngine* inProcessQmlEngine() const;

    virtual qint64 applicationProcessId() const = 0;

    QVariantMap systemProperties() const;

    virtual bool start() = 0;
    virtual void stop(bool forceKill = false) = 0;

    static RuntimeSignaler* signaler();

signals:
    void stateChanged(QT_PREPEND_NAMESPACE_AM(Am::RunState) newState);
    void finished(int exitCode, Am::ExitStatus status);

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

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(AbstractRuntime *))
