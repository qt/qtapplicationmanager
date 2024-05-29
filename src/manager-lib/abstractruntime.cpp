// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QUuid>

#include "global.h"
#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "exception.h"

/*!
    \qmltype Runtime
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a runtime that is executing an application.

    While an \l{ApplicationObject}{Application} is running, the associated Runtime object will be
    valid and yield access to runtime related information.

    Right now, only the accessor property for the \l Container object is exposed to the QML side.
*/
/*!
    \qmlproperty Container Runtime::container
    \readonly

    This property returns the \l Container object of a running application. Please see the \l{Containers}
    {general Container} and the \l Container class documentation for more information on containers
    within the application manager.
*/

QT_BEGIN_NAMESPACE_AM

AbstractRuntime::AbstractRuntime(AbstractContainer *container, Application *app, AbstractRuntimeManager *manager)
    : QObject(manager)
    , m_container(container)
    , m_app(app)
    , m_manager(manager)
{
    Q_STATIC_ASSERT(SecurityTokenSize == sizeof(QUuid));
    m_securityToken = QUuid::createUuid().toRfc4122();
}

QVariantMap AbstractRuntime::configuration() const
{
    if (m_manager)
        return m_manager->configuration();
    return QVariantMap();
}

QVariantMap AbstractRuntime::systemProperties() const
{
    if (m_manager && m_app)
        return m_app->isBuiltIn() ? m_manager->systemPropertiesBuiltIn() : m_manager->systemProperties3rdParty();
    return QVariantMap();
}

RuntimeSignaler *AbstractRuntime::signaler()
{
    static RuntimeSignaler rs;
    return &rs;
}

QByteArray AbstractRuntime::securityToken() const
{
    return m_securityToken;
}

void AbstractRuntime::openDocument(const QString &document, const QString &mimeType)
{
    Q_UNUSED(document)
    Q_UNUSED(mimeType)
}

void AbstractRuntime::setSlowAnimations(bool slow)
{
    // not every runtime needs this information
    Q_UNUSED(slow)
}

Application *AbstractRuntime::application() const
{
    return m_app.data();
}

AbstractRuntime::~AbstractRuntime()
{
    delete m_container;
}

AbstractRuntimeManager *AbstractRuntime::manager() const
{
    return m_manager;
}

bool AbstractRuntime::isQuickLauncher() const
{
    return false;
}

bool AbstractRuntime::attachApplicationToQuickLauncher(Application *app)
{
    Q_UNUSED(app)
    return false;
}

Am::RunState AbstractRuntime::state() const
{
    return m_state;
}

void AbstractRuntime::setState(Am::RunState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

void AbstractRuntime::setInProcessQmlEngine(QQmlEngine *engine)
{
    m_inProcessQmlEngine = engine;
}

QQmlEngine *AbstractRuntime::inProcessQmlEngine() const
{
    return m_inProcessQmlEngine;
}

AbstractContainer *AbstractRuntime::container() const
{
    return m_container;
}

AbstractRuntimeManager::AbstractRuntimeManager(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{ }

QString AbstractRuntimeManager::defaultIdentifier()
{
    return QString();
}

QString AbstractRuntimeManager::identifier() const
{
    return m_id;
}

bool AbstractRuntimeManager::inProcess() const
{
    return false;
}

bool AbstractRuntimeManager::supportsQuickLaunch() const
{
    return false;
}

QVariantMap AbstractRuntimeManager::configuration() const
{
    return m_configuration;
}

void AbstractRuntimeManager::setConfiguration(const QVariantMap &configuration)
{
    m_configuration = configuration;
}

QVariantMap AbstractRuntimeManager::systemPropertiesBuiltIn() const
{
    return m_systemPropertiesBuiltIn;
}

QVariantMap AbstractRuntimeManager::systemProperties3rdParty() const
{
    return m_systemProperties3rdParty;
}

void AbstractRuntimeManager::setSystemProperties(const QVariantMap &thirdParty, const QVariantMap &builtIn)
{
    m_systemProperties3rdParty = thirdParty;
    m_systemPropertiesBuiltIn = builtIn;
}

OpenGLConfiguration AbstractRuntimeManager::systemOpenGLConfiguration() const
{
    return m_systemOpenGLConfiguration;
}

void AbstractRuntimeManager::setSystemOpenGLConfiguration(const OpenGLConfiguration &openGLConfiguration)
{
    m_systemOpenGLConfiguration = openGLConfiguration;
}

QStringList AbstractRuntimeManager::iconThemeSearchPaths() const
{
    return m_iconThemeSearchPaths;
}

QString AbstractRuntimeManager::iconThemeName() const
{
    return m_iconThemeName;
}

void AbstractRuntimeManager::setIconTheme(const QStringList &themeSearchPaths, const QString &themeName)
{
    m_iconThemeSearchPaths = themeSearchPaths;
    m_iconThemeName = themeName;
}

QT_END_NAMESPACE_AM

#include "moc_abstractruntime.cpp"
