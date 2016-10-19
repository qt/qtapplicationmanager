/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QString>
#include <QVariantMap>

#include <QtAppManCommon/exception.h>

QT_BEGIN_NAMESPACE_AM

class InstallationLocation
{
public:
    enum Type {
        Invalid  = -1,
        Internal,
        Removable,
    };

    static const InstallationLocation invalid;

    bool operator==(const InstallationLocation &other) const;
    inline bool operator!=(const InstallationLocation &other) const { return !((*this) == other); }

    QString id() const;
    Type type() const;
    int index() const;

    QString installationPath() const;
    QString documentPath() const;

    bool isValid() const;
    bool isDefault() const;
    bool isRemovable() const;
    bool isMounted() const;

    QVariantMap toVariantMap() const;

    QString mountPoint() const; // debug only / not exported to QVariantMap

    static Type typeFromString(const QString &str);
    static QString typeToString(Type type);

    static QVector<InstallationLocation> parseInstallationLocations(const QVariantList &list) throw (Exception);

private:
    Type m_type = Invalid;
    int m_index = 0;
    bool m_isDefault = false;
    QString m_installationPath;
    QString m_documentPath;
    QString m_mountPoint;
};

QT_END_NAMESPACE_AM
