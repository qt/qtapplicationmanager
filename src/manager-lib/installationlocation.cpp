/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QDir>

#include "installationlocation.h"
#include "global.h"
#include "utilities.h"
#include "exception.h"


static QString fixPath(const QString &path)
{
    QString realPath = path;
    realPath.replace(qL1S("@HARDWARE-ID@"), hardwareId());
    QDir dir(realPath);
    return (dir.exists() ? dir.canonicalPath() : dir.absolutePath()) + qL1C('/');
}

bool InstallationLocation::operator==(const InstallationLocation &other) const
{
    return (m_type == other.m_type)
            && (m_index == other.m_index)
            && (m_installationPath == other.m_installationPath)
            && (m_documentPath == other.m_documentPath)
            && (m_mountPoint == other.m_mountPoint);
}

InstallationLocation::Type InstallationLocation::typeFromString(const QString &str)
{
    for (Type t: { Invalid, Internal, Removable}) {
        if (typeToString(t) == str)
            return t;
    }
    return Invalid;
}

QString InstallationLocation::typeToString(Type type)
{
    switch (type) {
    default:
    case Invalid: return qSL("invalid");
    case Internal: return qSL("internal");
    case Removable: return qSL("removable");
    }
}

QString InstallationLocation::id() const
{
    QString name = typeToString(m_type);
    if (m_type != Invalid)
        name = name + QLatin1Char('-') + QString::number(m_index);
    return name;
}

InstallationLocation::Type InstallationLocation::type() const
{
    return m_type;
}

int InstallationLocation::index() const
{
    return m_index;
}

bool InstallationLocation::isValid() const
{
    return m_type != Invalid;
}

bool InstallationLocation::isDefault() const
{
    return m_isDefault;
}

bool InstallationLocation::isRemovable() const
{
    return m_type == Removable;
}

bool InstallationLocation::isMounted() const
{
    if (!isRemovable())
        return true;
    else if (m_mountPoint.isEmpty())
        return false;
    else
        return mountedDirectories().uniqueKeys().contains(QDir(m_mountPoint).canonicalPath());
}

QString InstallationLocation::installationPath() const
{
    return m_installationPath;
}

QString InstallationLocation::documentPath() const
{
    return m_documentPath;
}

QVariantMap InstallationLocation::toVariantMap() const
{
    QVariantMap map;
    map[qSL("id")] = id();
    map[qSL("type")] = typeToString(type());
    map[qSL("index")] = index();
    map[qSL("installationPath")] = installationPath();
    map[qSL("documentPath")] = documentPath();
    map[qSL("isRemovable")] = isRemovable();
    map[qSL("isDefault")] = isDefault();

    bool mounted = isMounted();

    quint64 total = 0, free = 0;
    if (mounted)
        diskUsage(installationPath(), &total, &free);

    map[qSL("isMounted")] = mounted;
    map[qSL("installationDeviceSize")] = total;
    map[qSL("installationDeviceFree")] = free;

    total = free = 0;
    if (mounted)
        diskUsage(documentPath(), &total, &free);

    map[qSL("documentDeviceSize")] = total;
    map[qSL("documentDeviceFree")] = free;

    return map;
}

QString InstallationLocation::mountPoint() const
{
    return m_mountPoint;
}

QVector<InstallationLocation> InstallationLocation::parseInstallationLocations(const QVariantList &list) throw (Exception)
{
    QVector<InstallationLocation> locations;
    bool gotDefault = false;

    foreach (const QVariant &v, list) {
        QVariantMap map = v.toMap();

        QString id = map.value(qSL("id")).toString();
        QString instPath = map.value(qSL("installationPath")).toString();
        QString documentPath = map.value(qSL("documentPath")).toString();
        QString mountPoint = map.value(qSL("mountPoint")).toString();
        bool isDefault = map.value(qSL("isDefault")).toBool();

        if (isDefault) {
            if (!gotDefault)
                gotDefault = true;
            else
                throw Exception(Error::Parse, "multiple default installation locations defined");
        }

        Type type = InstallationLocation::typeFromString(id.section('-', 0, 0));
        bool ok = false;
        int index = id.section('-', 1).toInt(&ok);

        if ((type != Invalid) && (index >= 0) && ok) {
            InstallationLocation il;
            il.m_type = type;
            il.m_index = index;
            il.m_installationPath = fixPath(instPath);
            il.m_documentPath = fixPath(documentPath);
            il.m_mountPoint = mountPoint;
            il.m_isDefault = isDefault;

            //RG: should we disallow Removable locations to be the default location?

            if (!il.isRemovable()) {
                if (!QDir::root().mkpath(instPath))
                    throw Exception(Error::Parse, "the app directory %2 for the installation location %1 does not exist although the location is not removable").arg(id).arg(instPath);
                if (!QDir::root().mkpath(documentPath))
                    throw Exception(Error::Parse, "the doc directory %2 for the installation location %1 does not exist although the location is not removable").arg(id).arg(documentPath);
            }
            locations.append(il);
        } else {
            throw Exception(Error::Parse, "could not parse the installation location with id %1").arg(id);
        }
    }

    if (locations.isEmpty())
        throw Exception(Error::Parse, "no installation locations defined in config file");

    return locations;
}
