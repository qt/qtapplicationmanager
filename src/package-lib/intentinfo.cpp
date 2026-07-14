// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentinfo.h"
#include "packageinfo.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM


IntentInfo::IntentInfo(PackageInfo *packageInfo)
    : m_packageInfo(packageInfo)
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

QMap<QString, QString> IntentInfo::descriptions() const
{
    return m_descriptions.isEmpty() ? m_packageInfo->descriptions() : m_descriptions;
}

QString IntentInfo::icon() const
{
    return m_icon.isEmpty() ? m_packageInfo->icon() : m_icon;
}

bool IntentInfo::handleOnlyWhenRunning() const
{
    return m_handleOnlyWhenRunning;
}

QT_END_NAMESPACE_AM
