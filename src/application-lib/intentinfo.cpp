/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
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

#include <QDataStream>

#include "intentinfo.h"
#include "packageinfo.h"

QT_BEGIN_NAMESPACE_AM


IntentInfo::IntentInfo(PackageInfo *packageInfo)
    : m_packageInfo(packageInfo)
{ }

IntentInfo::~IntentInfo()
{ }

QString IntentInfo::id() const
{
    return m_id;
}

IntentInfo::Visibility IntentInfo::visibility() const
{
    return m_visibility;
}

QStringList IntentInfo::requiredCapabilities() const
{
    return m_requiredCapabilities;
}

QVariantMap IntentInfo::parameterMatch() const
{
    return m_parameterMatch;
}

QString IntentInfo::handlingApplicationId() const
{
    return m_handlingApplicationId;
}

QStringList IntentInfo::categories() const
{
    return m_categories.isEmpty() ? m_packageInfo->categories() : m_categories;
}

QMap<QString, QString> IntentInfo::names() const
{
    return m_names.isEmpty() ? m_packageInfo->names() : m_names;
}

QString IntentInfo::name(const QString &language) const
{
    return m_names.isEmpty() ? m_packageInfo->name(language) : m_names.value(language);
}

QMap<QString, QString> IntentInfo::descriptions() const
{
    return m_descriptions;
}

QString IntentInfo::description(const QString &language) const
{
    return m_descriptions.value(language);
}

QString IntentInfo::icon() const
{
    return m_icon.isEmpty() ? m_packageInfo->icon() : m_icon;
}

void IntentInfo::writeToDataStream(QDataStream &ds) const
{
    ds << m_id
       << (m_visibility == Public ? qSL("public") : qSL("private"))
       << m_requiredCapabilities
       << m_parameterMatch
       << m_handlingApplicationId
       << m_categories
       << m_names
       << m_descriptions
       << m_icon;
}

IntentInfo *IntentInfo::readFromDataStream(PackageInfo *pkg, QDataStream &ds)
{
    QScopedPointer<IntentInfo> intent(new IntentInfo(pkg));
    QString visibilityStr;

    ds >> intent->m_id
       >> visibilityStr
       >> intent->m_requiredCapabilities
       >> intent->m_parameterMatch
       >> intent->m_handlingApplicationId
       >> intent->m_categories
       >> intent->m_names
       >> intent->m_descriptions
       >> intent->m_icon;

    intent->m_visibility = (visibilityStr == qSL("public")) ? Public : Private;
    intent->m_categories.sort();

    return intent.take();
}

QT_END_NAMESPACE_AM
