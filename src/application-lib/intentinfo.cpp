// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDataStream>

#include "intentinfo.h"
#include "packageinfo.h"

#include <memory>

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

quint32 IntentInfo::dataStreamVersion()
{
    return 3;
}

void IntentInfo::writeToDataStream(QDataStream &ds) const
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    ds << m_id
       << (m_visibility == Public ? qSL("public") : qSL("private"))
       << m_requiredCapabilities
       << m_parameterMatch
       << m_handlingApplicationId
       << m_categories
       << m_names
       << m_descriptions
       << m_icon
       << m_handleOnlyWhenRunning;
}

IntentInfo *IntentInfo::readFromDataStream(PackageInfo *pkg, QDataStream &ds)
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    auto intent = std::make_unique<IntentInfo>(pkg);
    QString visibilityStr;

    ds >> intent->m_id
       >> visibilityStr
       >> intent->m_requiredCapabilities
       >> intent->m_parameterMatch
       >> intent->m_handlingApplicationId
       >> intent->m_categories
       >> intent->m_names
       >> intent->m_descriptions
       >> intent->m_icon
       >> intent->m_handleOnlyWhenRunning;

    intent->m_visibility = (visibilityStr == qSL("public")) ? Public : Private;
    intent->m_categories.sort();

    return intent.release();
}

QT_END_NAMESPACE_AM
