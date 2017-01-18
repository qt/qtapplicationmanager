/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include "global.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "cryptography.h"
#include "exception.h"

QT_BEGIN_NAMESPACE_AM

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

QVariantMap AbstractRuntime::systemProperties() const
{
    if (m_manager && m_app)
        return m_app->isBuiltIn() ? m_manager->systemPropertiesBuiltIn() : m_manager->systemProperties3rdParty();
    return QVariantMap();
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

bool AbstractRuntime::needsLauncher() const
{
    return false;
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

AbstractRuntime::State AbstractRuntime::state() const
{
    return m_state;
}

void AbstractRuntime::setState(AbstractRuntime::State newState)
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

QT_END_NAMESPACE_AM
