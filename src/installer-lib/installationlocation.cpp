/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include <QDir>

#include "installationlocation.h"
#include "global.h"
#include "utilities.h"
#include "exception.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <errno.h>
#  if defined(Q_OS_ANDROID)
#    include <sys/vfs.h>
#    define statvfs statfs
#  else
#    include <sys/statvfs.h>
#  endif
#endif

QT_BEGIN_NAMESPACE_AM

static QString fixPath(const QString &path, const QString &hardwareId)
{
    QString realPath = path;
    realPath.replace(qL1S("@HARDWARE-ID@"), hardwareId);
    QDir dir(realPath);
    return (dir.exists() ? dir.canonicalPath() : dir.absolutePath()) + qL1C('/');
}

static bool diskUsage(const QString &path, quint64 *bytesTotal, quint64 *bytesFree)
{
    QString cpath = QFileInfo(path).canonicalPath();

#if defined(Q_OS_WIN)
    return GetDiskFreeSpaceExW((LPCWSTR) cpath.utf16(), (ULARGE_INTEGER *) bytesFree,
                               (ULARGE_INTEGER *) bytesTotal, nullptr);

#else // Q_OS_UNIX
    int result;
    struct ::statvfs svfs;

    do {
        result = ::statvfs(cpath.toLocal8Bit(), &svfs);
        if (result == -1 && errno == EINTR)
            continue;
    } while (false);

    if (result == 0) {
        if (bytesTotal)
            *bytesTotal = quint64(svfs.f_frsize) * svfs.f_blocks;
        if (bytesFree)
            *bytesFree = quint64(svfs.f_frsize) * svfs.f_bavail;
        return true;
    }
    return false;
#endif // Q_OS_WIN
}


bool InstallationLocation::operator==(const InstallationLocation &other) const
{
    return (m_valid == other.m_valid)
            && (m_index == other.m_index)
            && (m_installationPath == other.m_installationPath)
            && (m_documentPath == other.m_documentPath);
}

QString InstallationLocation::id() const
{
    QString name = m_valid ? qSL("internal") : qSL("invalid");
    if (m_valid)
        name = name + QLatin1Char('-') + QString::number(m_index);
    return name;
}

int InstallationLocation::index() const
{
    return m_index;
}

bool InstallationLocation::isValid() const
{
    return m_valid;
}

bool InstallationLocation::isDefault() const
{
    return m_isDefault;
}

QString InstallationLocation::installationPath() const
{
    return m_installationPath;
}

QString InstallationLocation::documentPath() const
{
    return m_documentPath;
}

bool InstallationLocation::installationDeviceFreeSpace(quint64 *bytesTotal, quint64 *bytesFree) const
{
    return diskUsage(installationPath(), bytesTotal, bytesFree);
}

bool InstallationLocation::documentDeviceFreeSpace(quint64 *bytesTotal, quint64 *bytesFree) const
{
    return diskUsage(documentPath(), bytesTotal, bytesFree);
}

QVariantMap InstallationLocation::toVariantMap() const
{
    QVariantMap map;
    map[qSL("id")] = id();
    map[qSL("type")] = m_valid ? qSL("internal") : qSL("invalid");
    map[qSL("index")] = index();
    map[qSL("installationPath")] = installationPath();
    map[qSL("documentPath")] = documentPath();
    map[qSL("isDefault")] = isDefault();

    quint64 total = 0, free = 0;
    installationDeviceFreeSpace(&total, &free);

    map[qSL("installationDeviceSize")] = total;
    map[qSL("installationDeviceFree")] = free;

    total = free = 0;
    documentDeviceFreeSpace(&total, &free);

    map[qSL("documentDeviceSize")] = total;
    map[qSL("documentDeviceFree")] = free;

    return map;
}

QVector<InstallationLocation> InstallationLocation::parseInstallationLocations(const QVariantList &list,
                                                                               const QString &hardwareId) Q_DECL_NOEXCEPT_EXPR(false)
{
    QVector<InstallationLocation> locations;
    bool gotDefault = false;

    for (const QVariant &v : list) {
        QVariantMap map = v.toMap();

        QString id = map.value(qSL("id")).toString();
        QString instPath = map.value(qSL("installationPath")).toString();
        QString documentPath = map.value(qSL("documentPath")).toString();
        bool isDefault = map.value(qSL("isDefault")).toBool();

        if (isDefault) {
            if (!gotDefault)
                gotDefault = true;
            else
                throw Exception(Error::Parse, "multiple default installation locations defined");
        }

        QString type = id.section('-', 0, 0);
        bool ok = false;
        int index = id.section('-', 1).toInt(&ok);

        if ((type == qSL("internal")) && (index >= 0) && ok) {
            InstallationLocation il;
            il.m_valid = true;
            il.m_index = index;
            il.m_installationPath = fixPath(instPath, hardwareId);
            il.m_documentPath = fixPath(documentPath, hardwareId);
            il.m_isDefault = isDefault;

            if (!QDir::root().mkpath(instPath)) {
                throw Exception(Error::Parse, "the app directory %2 for the installation location %1 does not exist")
                        .arg(id).arg(instPath);
            }
            if (!QDir::root().mkpath(documentPath)) {
                throw Exception(Error::Parse, "the doc directory %2 for the installation location %1 does not exist")
                    .arg(id).arg(documentPath);
            }

            locations.append(il);
        } else {
            throw Exception(Error::Parse, "could not parse the installation location with id %1").arg(id);
        }
    }
    return locations;
}

QT_END_NAMESPACE_AM
