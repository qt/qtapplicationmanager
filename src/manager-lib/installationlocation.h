/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QString>
#include <QVariantMap>

#include "exception.h"


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

    static QList<InstallationLocation> parseInstallationLocations(const QVariantList &list) throw (Exception);

private:
    Type m_type = Invalid;
    int m_index = 0;
    bool m_isDefault = false;
    QString m_installationPath;
    QString m_documentPath;
    QString m_mountPoint;
};
