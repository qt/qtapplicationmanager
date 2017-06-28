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

#include <qplatformdefs.h>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

#include "notificationmanagerdbuscontextadaptor.h"
#include "notificationmanager.h"
#include "org.freedesktop.notifications_adaptor.h"

QT_BEGIN_NAMESPACE_AM

NotificationManagerDBusContextAdaptor::NotificationManagerDBusContextAdaptor(NotificationManager *nm)
    : AbstractDBusContextAdaptor(nm)
{
    m_adaptor = new NotificationsAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

NotificationsAdaptor::NotificationsAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto nm = NotificationManager::instance();

    connect(nm, &NotificationManager::ActionInvoked,
            this, &NotificationsAdaptor::ActionInvoked);
    connect(nm, &NotificationManager::NotificationClosed,
            this, &NotificationsAdaptor::NotificationClosed);
}

NotificationsAdaptor::~NotificationsAdaptor()
{ }

void NotificationsAdaptor::CloseNotification(uint id)
{
    NotificationManager::instance()->CloseNotification(id);
}

QStringList NotificationsAdaptor::GetCapabilities()
{
    return NotificationManager::instance()->GetCapabilities();
}

QString NotificationsAdaptor::GetServerInformation(QString &vendor, QString &version, QString &spec_version)
{
    return NotificationManager::instance()->GetServerInformation(vendor, version, spec_version);
}

uint NotificationsAdaptor::Notify(const QString &app_name, uint replaces_id, const QString &app_icon, const QString &summary, const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout)
{
    return NotificationManager::instance()->Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, timeout);
}

