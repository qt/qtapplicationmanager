/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "intent.h"
#include "utilities.h"

#include <QRegularExpression>
#include <QVariant>
#include <QLocale>

QT_BEGIN_NAMESPACE_AM


/*!
    \qmltype IntentObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief This type represents an Intent definition on the System UI side.

    Each instance of this class represents a single Intent definition for a specific application.

    Most of the read-only properties map directly to values read from the package's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.

    Items of this type are not creatable from QML code. Only functions and properties of
    IntentServer and IntentModel will return pointers to this class.

    Make sure to \b not save references to an IntentObject across function calls: packages (and
    with that, the intents contained within) can be deinstalled at any time, invalidating your
    reference. In case you do need a persistent handle, use the intentId together with the
    applicationId string.
*/

/*! \qmlproperty string IntentObject::intentId
    \readonly
    The id of the intent.
*/

/*! \qmlproperty string IntentObject::packageId
    \readonly
    The id of the package that the handling application of this intent is part of.
*/

/*! \qmlproperty string IntentObject::applicationId
    \readonly
    The id of the application responsible for handling this intent.
*/

/*! \qmlproperty IntentObject.Visibility IntentObject::visibility
    \readonly
    The visibility of this intent for other packages.

    \list
    \li IntentObject.Public - Any application can request this intent.
    \li IntentObject.Private - Only applications from the same package can request this intent.
    \endlist
*/

/*! \qmlproperty list<string> IntentObject::requiredCapabilities
    \readonly
    An \l{ApplicationObject}{application} requesting this intent needs to have all of the given
    capabilities.

    \sa ApplicationObject::capabilities
*/

/*! \qmlproperty var IntentObject::parameterMatch
    \readonly
    A handling application can limit what parameter values it accepts. The property itself is an
    object that corresponds to a subset of allowed parameter object of this intent.
    When set, the parameters of each incoming intent request are matched against this object,
    following these rules:
    \list
    \li a field missing from \c parameterMatch is ignored.
    \li a field of type \c string specified in \c parameterMatch is matched as a regular
        expressions against the corresponding parameter value.
    \li for fields of type \c list specified in \c parameterMatch, the corresponding parameter value
        has to match any of the values in the list (using QVariant compare).
    \li any other fields in \c parameterMatch are compared as QVariants to the corresponding
        parameter value.
    \endlist

    One example would be an \c open-mime-type intent that is implemented by many applications: there
    would be a \c mimeType parameter and each application could limit the requests it wants to
    receive by setting a parameterMatch on this \c mimeType parameter, e.g.
    \c{{ mimeType: "^image/.*\.png$" }}
*/
/*!
    \qmlproperty url IntentObject::icon
    \readonly

    The URL of the intent's icon - can be used as the source property of an \l Image.
    If the intent does not specify an \c icon, this will return the same as the containing
    PackageObject::icon.
*/
/*!
    \qmlproperty string IntentObject::name
    \readonly

    Returns the localized name of the intent - as provided in the info.yaml file - in the currently
    active locale.

    This is a convenience property, that takes the mapping returned by the \l names property,
    and then tries to return the value for these keys if available: first the current locale's id,
    then \c en_US, then \c en and lastly the first available key.

    If no mapping is available, this will return the \l intentId.
*/
/*!
    \qmlproperty var IntentObject::names
    \readonly

    Returns an object with all the language code to localized name mappings as provided in the
    intent's info.yaml file. If the intent does not specify a \c names object, this will
    return the same as the containing PackageObject::names.
*/
/*!
    \qmlproperty string IntentObject::description
    \readonly

    Returns the localized description of the intent - as provided in the info.yaml file - in the
    currently active locale.

    This property uses the same algorithm as the \l name property, but for the description.
*/
/*!
    \qmlproperty var IntentObject::descriptions
    \readonly

    Returns an object with all the language code to localized description mappings as provided in
    the intent's info.yaml file. If the intent does not specify a \c descriptions object,
    this will return the same as the containing PackageObject::descriptions.
*/
/*!
    \qmlproperty list<string> IntentObject::categories
    \readonly

    A list of category names the intent should be associated with. This is mainly for displaying
    the intent within a fixed set of categories in the System UI.
    If the intent does not specify a \c categories list, this will return the same as the
    containing PackageObject::categories.
*/


Intent::Intent()
{ }

Intent::Intent(const QString &id, const QString &packageId, const QString &applicationId,
               const QStringList &capabilities, Intent::Visibility visibility,
               const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
               const QMap<QString, QString> &descriptions, const QUrl &icon,
               const QStringList &categories)
    : m_intentId(id)
    , m_visibility(visibility)
    , m_requiredCapabilities(capabilities)
    , m_parameterMatch(parameterMatch)
    , m_packageId(packageId)
    , m_applicationId(applicationId)
    , m_names(names)
    , m_descriptions(descriptions)
    , m_categories(categories)
    , m_icon(icon)
{
}

QString Intent::intentId() const
{
    return m_intentId;
}

Intent::Visibility Intent::visibility() const
{
    return m_visibility;
}

QStringList Intent::requiredCapabilities() const
{
    return m_requiredCapabilities;
}

QVariantMap Intent::parameterMatch() const
{
    return m_parameterMatch;
}

QString Intent::packageId() const
{
    return m_packageId;
}

QString Intent::applicationId() const
{
    return m_applicationId;
}

bool Intent::checkParameterMatch(const QVariantMap &parameters) const
{
    for (auto rit = m_parameterMatch.cbegin(); rit != m_parameterMatch.cend(); ++rit) {
        const QString &paramName = rit.key();
        auto pit = parameters.find(paramName);
        if (pit == parameters.cend())
            return false;

        const QVariant requiredValue = rit.value();
        const QVariant actualValue = pit.value();

        switch (requiredValue.metaType().id()) {
        case QMetaType::QString: {
            QRegularExpression regexp(requiredValue.toString());
            auto match = regexp.match(actualValue.toString());
            if (!match.hasMatch())
                return false;
            break;
        }
        case QMetaType::QVariantList: {
            bool foundMatch = false;
            const QVariantList rvlist = requiredValue.toList();
            for (const QVariant &rv2 : rvlist) {
                if (actualValue.canConvert(rv2.metaType()) && actualValue == rv2) {
                    foundMatch = true;
                    break;
                }
            }
            if (!foundMatch)
                return false;
            break;
        }
        default: {
            if (requiredValue != actualValue)
                return false;
            break;
        }
        }
    }
    return true;
}

QUrl Intent::icon() const
{
    return m_icon;
}

QString Intent::name() const
{
    return translateFromMap(m_names, intentId());
}

QVariantMap Intent::names() const
{
    QVariantMap vm;
    for (auto it = m_names.cbegin(); it != m_names.cend(); ++it)
        vm.insert(it.key(), it.value());
    return vm;
}

QString Intent::description() const
{
    return translateFromMap(m_descriptions);
}

QVariantMap Intent::descriptions() const
{
    QVariantMap vm;
    for (auto it = m_descriptions.cbegin(); it != m_descriptions.cend(); ++it)
        vm.insert(it.key(), it.value());
    return vm;
}

QStringList Intent::categories() const
{
    return m_categories;
}

QT_END_NAMESPACE_AM

#include "moc_intent.cpp"
