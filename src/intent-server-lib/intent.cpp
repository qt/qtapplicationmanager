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

#include "intent.h"

#include <QRegularExpression>
#include <QVariant>
#include <QLocale>

QT_BEGIN_NAMESPACE_AM


/*!
    \qmltype IntentObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief This type represents an Intent definition on the System-UI side.

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


Intent::Intent()
{ }

Intent::Intent(const QString &id, const QString &packageId, const QString &applicationId,
               const QStringList &capabilities, Intent::Visibility visibility,
               const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
               const QUrl &icon, const QStringList &categories)
    : m_intentId(id)
    , m_visibility(visibility)
    , m_requiredCapabilities(capabilities)
    , m_parameterMatch(parameterMatch)
    , m_packageId(packageId)
    , m_applicationId(applicationId)
    , m_categories(categories)
    , m_icon(icon)
{
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        m_names.insert(it.key(), it.value());
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
    QMapIterator<QString, QVariant> rit(m_parameterMatch);
    while (rit.hasNext()) {
        rit.next();
        const QString &paramName = rit.key();
        auto pit = parameters.find(paramName);
        if (pit == parameters.cend())
            return false;

        const QVariant requiredValue = rit.value();
        const QVariant actualValue = pit.value();

        switch (requiredValue.type()) {
        case QVariant::String: {
            QRegularExpression regexp(requiredValue.toString());
            auto match = regexp.match(actualValue.toString());
            if (!match.hasMatch())
                return false;
            break;
        }
        case QVariant::List: {
            bool foundMatch = false;
            const QVariantList rvlist = requiredValue.toList();
            for (const QVariant &rv2 : rvlist) {
                if (actualValue.canConvert(int(rv2.type())) && actualValue == rv2) {
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
    QVariant name;
    if (!m_names.isEmpty()) {
        name = m_names.value(QLocale::system().name()); //TODO: language changes
        if (name.isNull())
            name = m_names.value(qSL("en"));
        if (name.isNull())
            name = m_names.value(qSL("en_US"));
        if (name.isNull())
            name = *m_names.constBegin();
    } else {
        name = intentId();
    }
    return name.toString();
}

QVariantMap Intent::names() const
{
    return m_names;
}

QStringList Intent::categories() const
{
    return m_categories;
}

QT_END_NAMESPACE_AM
