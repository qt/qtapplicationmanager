// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QUuid>

#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "globalruntimeconfiguration.h"
#include "exception.h"

/*!
    \qmltype Runtime
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a runtime that is executing an application.

    While an \l{ApplicationObject}{Application} is running, the associated Runtime object will be
    valid and yield access to runtime related information.
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

    AbstractRuntimeManager::s_allRuntimes.append(this);
}

QVariantMap AbstractRuntime::configuration() const
{
    if (m_manager)
        return m_manager->configuration();
    return { };
}

QVariantMap AbstractRuntime::systemProperties() const
{
    if (m_app) {
        const auto &grc = GlobalRuntimeConfiguration::instance();
        return m_app->isBuiltIn() ? grc.systemPropertiesForBuiltInApps
                                  : grc.systemPropertiesForThirdPartyApps;
    }
    return { };
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

void AbstractRuntime::setApplicationExtraDirs(const QMap<QString, QString> &extraPaths)
{
    Q_UNUSED(extraPaths);
}

Application *AbstractRuntime::application() const
{
    return m_app.data();
}

AbstractRuntime::~AbstractRuntime()
{
    delete m_container;
    AbstractRuntimeManager::s_allRuntimes.removeOne(this);
}

AbstractRuntimeManager *AbstractRuntime::manager() const
{
    return m_manager;
}


/*!
    \qmlproperty string Runtime::runtimeId
    \readonly
    \since 6.10

    This property returns the \c id of the runtime that is executing the application. The \c id
    is a unique identifier for the runtime integration and can be used to reference it in other
    parts of the System UI or in configuration files.
*/
QString AbstractRuntime::runtimeId() const
{
    return m_manager->identifier();
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

QList<AbstractRuntime *> AbstractRuntimeManager::s_allRuntimes;

AbstractRuntimeManager::AbstractRuntimeManager(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{ }

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

QList<AbstractRuntime *> AbstractRuntimeManager::fromProcessId(qint64 pid)
{
    QList<AbstractRuntime *> result;

    for (AbstractRuntime *runtime : std::as_const(s_allRuntimes)) {
        if (runtime->applicationProcessId() == pid)
            result << runtime;
    }
    return result;
}

QT_END_NAMESPACE_AM

#include "moc_abstractruntime.cpp"
