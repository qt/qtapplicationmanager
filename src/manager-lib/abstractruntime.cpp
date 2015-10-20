/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include "global.h"
#include "application.h"
#include "abstractruntime.h"
#include "cryptography.h"
#include "exception.h"

const Q_PID AbstractRuntime::INVALID_PID = Q_PID(-1);

QVariantMap AbstractRuntime::s_config;

AbstractRuntime::AbstractRuntime(QObject *parent)
    : QObject(parent)
    , m_app(0)
{
    m_securityToken = Cryptography::generateRandomBytes(SecurityTokenSize);
    if (m_securityToken.size() != SecurityTokenSize) {
        qCCritical(LogSystem) << "ERROR: Not enough entropy left to generate a security token - shuting down";
        abort();
    }
}

QVariantMap AbstractRuntime::configuration() const
{
    if (m_app)
        return s_config.value(m_app->runtimeName()).toMap();
    return QVariantMap();
}

bool AbstractRuntime::inProcess() const
{
    return false;
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
    if (m_app)
        m_app->setCurrentRuntime(0);
}

QString AbstractRuntime::identifier()
{
    return QString();
}

void AbstractRuntime::setInProcessQmlEngine(QQmlEngine *engine)
{
    m_inProcessQmlEngine = engine;
}

QQmlEngine *AbstractRuntime::inProcessQmlEngine() const
{
    return m_inProcessQmlEngine;
}

void AbstractRuntime::setConfiguration(const QVariantMap &config)
{
    s_config = config;
}
