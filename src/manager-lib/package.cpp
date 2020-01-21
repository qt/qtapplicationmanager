/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QLocale>

#include "package.h"
#include "packageinfo.h"
#include "applicationinfo.h"

QT_BEGIN_NAMESPACE_AM

Package::Package(PackageInfo *packageInfo, State initialState)
    : m_info(packageInfo)
    , m_state(initialState)
{ }

QString Package::id() const
{
    return info()->id();
}

bool Package::isBuiltIn() const
{
    return m_info->isBuiltIn();
}

bool Package::builtInHasRemovableUpdate() const
{
    return isBuiltIn() && m_updatedInfo;
}

QString Package::version() const
{
    return info()->version();
}

QString Package::name() const
{
    QString name;
    if (!info()->names().isEmpty()) {
        name = info()->name(QLocale::system().name()); //TODO: language changes
        if (name.isEmpty())
            name = info()->name(qSL("en"));
        if (name.isEmpty())
            name = info()->name(qSL("en_US"));
        if (name.isEmpty())
            name = *info()->names().constBegin();
    } else {
        name = id();
    }
    return name;
}

QVariantMap Package::names() const
{
    QVariantMap names;
    for (auto it = info()->names().cbegin(); it != info()->names().cend(); ++it)
        names.insert(it.key(), it.value());
    return names;
}

QString Package::description() const
{
    QString description;
    if (!info()->descriptions().isEmpty()) {
        description = info()->description(QLocale::system().name()); //TODO: language changes
        if (description.isEmpty())
            description = info()->description(qSL("en"));
        if (description.isEmpty())
            description = info()->description(qSL("en_US"));
        if (description.isEmpty())
            description = *info()->descriptions().constBegin();
    }
    return description;
}

QVariantMap Package::descriptions() const
{
    QVariantMap descriptions;
    for (auto it = info()->descriptions().cbegin(); it != info()->descriptions().cend(); ++it)
        descriptions.insert(it.key(), it.value());
    return descriptions;
}

QStringList Package::categories() const
{
    return info()->categories();
}

QUrl Package::icon() const
{
    if (info()->icon().isEmpty())
        return QUrl();

    QDir dir;
    switch (state()) {
    default:
    case Installed:
        dir = info()->baseDir();
        break;
    case BeingInstalled:
    case BeingUpdated:
        dir = QDir(info()->baseDir().absolutePath() + QLatin1Char('+'));
        break;
    case BeingRemoved:
        dir = QDir(info()->baseDir().absolutePath() + QLatin1Char('-'));
        break;
    }
    return QUrl::fromLocalFile(dir.absoluteFilePath(info()->icon()));
}

void Package::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void Package::setProgress(qreal progress)
{
    m_progress = progress;
}

PackageInfo *Package::info() const
{
    return m_updatedInfo ? m_updatedInfo : m_info;
}

PackageInfo *Package::baseInfo() const
{
    return m_info;
}

PackageInfo *Package::updatedInfo() const
{
    return m_updatedInfo;
}

PackageInfo *Package::setUpdatedInfo(PackageInfo *info)
{
    Q_ASSERT(!info || (m_info && info->id() == m_info->id()));
    Q_ASSERT(info != m_updatedInfo);

    auto old = m_updatedInfo;
    m_updatedInfo = info;
    emit bulkChange();
    return old;
}

PackageInfo *Package::setBaseInfo(PackageInfo *info)
{
    Q_ASSERT(info != m_info);

    auto old = m_info;
    m_info = info;
    emit bulkChange();
    return old;
}

bool Package::isBlocked() const
{
    return m_blocked > 0;
}

bool Package::block()
{
    bool blockedNow = (m_blocked.fetchAndAddOrdered(1) == 0);
    if (blockedNow) {
        m_blockedApps = info()->applications();
        m_blockedAppsCount = m_blockedApps.count();
        emit blockedChanged(true);
    }
    return blockedNow;
}

bool Package::unblock()
{
    bool unblockedNow = (m_blocked.fetchAndSubOrdered(1) == 1);
    if (unblockedNow) {
        m_blockedApps.clear();
        m_blockedAppsCount = 0;
        emit blockedChanged(false);
    }
    return unblockedNow;

}

void Package::applicationStoppedDueToBlock(const QString &appId)
{
    if (!isBlocked())
        return;

    auto it = std::find_if(m_blockedApps.cbegin(), m_blockedApps.cend(), [appId](const ApplicationInfo *appInfo) {
        return appInfo->id() == appId;
    });
    if (it != m_blockedApps.cend())
        m_blockedApps.removeOne(*it);
    m_blockedAppsCount = m_blockedApps.count();
}

bool Package::areAllApplicationsStoppedDueToBlock() const
{
    return isBlocked() && !m_blockedAppsCount;
}

QT_END_NAMESPACE_AM
