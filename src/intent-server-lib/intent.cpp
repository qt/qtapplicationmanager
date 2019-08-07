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

QT_BEGIN_NAMESPACE_AM


/*!
    \qmltype Intent
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief This type represents an Intent definition on the System-UI side.

    This is a QML gadget class representing a single Intent definition for a specific application.
    It is not creatable from QML code and all properties are read-only. Only functions and
    properties of IntentServer will return instances of this class.
*/

/*! \qmlproperty bool Intent::valid
    \readonly
    Set to \c true, if this instance reprensents a valid intent, or \c false otherwise.
*/

/*! \qmlproperty string Intent::intentId
    \readonly
    The id of the intent.
*/

/*! \qmlproperty string Intent::applicationId
    \readonly
    The id of the application that is handling this intent.
*/

/*! \qmlproperty Intent.Visibility Intent::visibility
    \readonly
    The visibility of this intent for other applications.

    \list
    \li Intent.Public - Any application can request this intent.
    \li Intent.Private - Only the handling application can request this intent (this will be more
                         useful once application background services have been implemented).
    \endlist
*/

/*! \qmlproperty list<string> Intent::requiredCapabilities
    \readonly
    An \l{ApplicationObject}{application} requesting this intent needs to have all of the given
    capabilities.

    \sa ApplicationObject::capabilities
*/

/*! \qmlproperty var Intent::parameterMatch
    \readonly
    A handling application can limit what parameter values it accepts. One example would be an
    open-mime-type intent that is implemented by many applications: there would be a \c mimeType
    parameter and each application could limit the requests it wants to receive by setting a
    parameterMatch on this \c mimeType parameter, e.g. \c{{ mimeType: "^image/.*\.png$" }}

*/


Intent::Intent()
{ }

Intent::Intent(const QString &id, const QString &applicationId, const QString &backgroundHandlerId,
               const QStringList &capabilities, Intent::Visibility visibility, const QVariantMap &parameterMatch)
    : m_intentId(id)
    , m_visibility(visibility)
    , m_requiredCapabilities(capabilities)
    , m_parameterMatch(parameterMatch)
    , m_applicationId(applicationId)
    , m_backgroundHandlerId(backgroundHandlerId)
{ }

Intent::operator bool() const
{
    return !m_intentId.isEmpty();
}

bool Intent::operator==(const Intent &other) const
{
    return (m_intentId == other.m_intentId)
            && (m_visibility == other.m_visibility)
            && (m_requiredCapabilities == other.m_requiredCapabilities)
            && (m_parameterMatch == other.m_parameterMatch)
            && (m_applicationId == other.m_applicationId)
            && (m_backgroundHandlerId == other.m_backgroundHandlerId);
}

bool Intent::operator <(const Intent &other) const
{
    return (m_intentId < other.m_intentId) ? true : (m_applicationId < other.m_applicationId);
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

QString Intent::applicationId() const
{
    return m_applicationId;
}

QString Intent::backgroundServiceId() const
{
    return m_backgroundHandlerId;
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
                if (actualValue.canConvert(rv2.type()) && actualValue == rv2) {
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

QT_END_NAMESPACE_AM
