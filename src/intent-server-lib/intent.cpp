// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
/*!
    \qmlproperty bool IntentObject::handleOnlyWhenRunning
    \readonly
    \since 6.5

    By default, applications are automatically started when a request is targeted at them, but
    they are not currently running. If this property is set to \c true, then any requests for this
    intent will only be forwarded to its handling application, if the application is actuallly
    running.
    This is useful for system-wide broadcasts that are only relevant if an application is active
    (e.g. changes in internet availability).
*/


Intent::Intent()
{ }

Intent::Intent(const QString &id, const QString &packageId, const QString &applicationId,
               const QStringList &capabilities, Intent::Visibility visibility,
               const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
               const QMap<QString, QString> &descriptions, const QUrl &icon,
               const QStringList &categories, bool handleOnlyWhenRunning)
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
    , m_handleOnlyWhenRunning(handleOnlyWhenRunning)
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

bool Intent::handleOnlyWhenRunning() const
{
    return m_handleOnlyWhenRunning;
}

QT_END_NAMESPACE_AM

#include "moc_intent.cpp"
