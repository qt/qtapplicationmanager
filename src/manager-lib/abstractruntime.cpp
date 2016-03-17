/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "global.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "cryptography.h"
#include "exception.h"


AbstractRuntime::AbstractRuntime(AbstractContainer *container, const Application *app, AbstractRuntimeManager *manager)
    : QObject(manager)
    , m_container(container)
    , m_app(app)
    , m_manager(manager)
{
    m_securityToken = Cryptography::generateRandomBytes(SecurityTokenSize);
    if (m_securityToken.size() != SecurityTokenSize) {
        qCCritical(LogSystem) << "ERROR: Not enough entropy left to generate a security token - shuting down";
        abort();
    }
}

QVariantMap AbstractRuntime::configuration() const
{
    if (m_manager)
        return m_manager->configuration();
    return QVariantMap();
}

QVariantMap AbstractRuntime::additionalConfiguration() const
{
    if (m_manager)
        return m_manager->additionalConfiguration();
    return QVariantMap();
}

QByteArray AbstractRuntime::securityToken() const
{
    return m_securityToken;
}

void AbstractRuntime::openDocument(const QString &document)
{
    Q_UNUSED(document)
}

const Application *AbstractRuntime::application() const
{
    return m_app;
}

AbstractRuntime::~AbstractRuntime()
{
    if (m_app && m_app->currentRuntime() == this)
        m_app->setCurrentRuntime(0);
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

bool AbstractRuntime::attachApplicationToQuickLauncher(const Application *app)
{
    Q_UNUSED(app)
    return false;
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

QVariantMap AbstractRuntimeManager::additionalConfiguration() const
{
    return m_additionalConfiguration;
}

void AbstractRuntimeManager::setAdditionalConfiguration(const QVariantMap &additionalConfiguration)
{
    m_additionalConfiguration = additionalConfiguration;
}
