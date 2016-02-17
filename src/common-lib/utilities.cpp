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

#include <QFileInfo>
#include <QFile>

#include "utilities.h"
#include "exception.h"

#include <errno.h>

#if defined(Q_OS_WIN)
#  include <windows.h>
#elif defined(Q_OS_OSX)
#  include <unistd.h>
#  include <sys/mount.h>
#  include <sys/statvfs.h>
#else
#  include <unistd.h>
#  include <stdio.h>
#  include <mntent.h>
#  include <sys/stat.h>
#  if defined(Q_OS_ANDROID)
#    include <sys/vfs.h>
#    define statvfs statfs
#  else
#    include <sys/statvfs.h>
#  endif
#endif

bool diskUsage(const QString &path, quint64 *bytesTotal, quint64 *bytesFree)
{
    QString cpath = QFileInfo(path).canonicalPath();

#if defined(Q_OS_WIN)
    return GetDiskFreeSpaceExW((LPCWSTR) cpath.utf16(), (ULARGE_INTEGER *) bytesFree, (ULARGE_INTEGER *) bytesTotal, 0);

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

QMultiMap<QString, QString> mountedDirectories()
{
    QMultiMap<QString, QString> result;
#if defined(Q_OS_WIN)
    return result; // no mounts on Windows

#elif defined(Q_OS_OSX)
    struct statfs *sfs = 0;
    int count = getmntinfo(&sfs, MNT_NOWAIT);

    for (int i = 0; i < count; ++i, ++sfs) {
        result.insertMulti(QString::fromLocal8Bit(sfs->f_mntonname), QString::fromLocal8Bit(sfs->f_mntfromname));
    }
#else
    QFile fpm(qSL("/proc/self/mounts"));
    if (!fpm.open(QFile::ReadOnly))
        return result;
    QByteArray mountBuffer = fpm.readAll();

    FILE *pm = fmemopen(mountBuffer.data(), mountBuffer.size(), "r");
    if (!pm)
        return result;

#  if defined(Q_OS_ANDROID)
    while (struct mntent *mntPtr = getmntent(pm)) {
        result.insertMulti(QString::fromLocal8Bit(mntPtr->mnt_dir),
                           QString::fromLocal8Bit(mntPtr->mnt_fsname));
    }
#  else
    int pathMax = pathconf("/", _PC_PATH_MAX);
    char strBuf[1024 + 2 * pathMax]; // quite big, but better be safe than sorry
    struct mntent mntBuf;

    while (getmntent_r(pm, &mntBuf, strBuf, sizeof(strBuf))) {
        result.insertMulti(QString::fromLocal8Bit(mntBuf.mnt_dir),
                           QString::fromLocal8Bit(mntBuf.mnt_fsname));
    }
#  endif
    fclose(pm);
#endif

    return result;
}


bool isValidDnsName(const QString &dnsName, bool isAliasName, QString *errorString)
{
    static const int maxLength = 150;

    try {
        // AM specific checks first
        // we need to make sure that we can use the name as directory on a FAT formatted SD-card,
        // which has a 255 character path length restriction

        if (dnsName.length() > maxLength)
            throw Exception(Error::Parse, "the maximum length is %1 characters (found %2 characters)").arg(maxLength, dnsName.length());

        // we require at least 3 parts: tld.company.app
        QStringList parts = dnsName.split('.');
        if (parts.size() < 3)
            throw Exception(Error::Parse, "the minimum amount of parts (subdomains) is 3 (found %1)").arg(parts.size());

        // standard RFC compliance tests (RFC 1035/1123)

        auto partCheck = [](const QString &part, const char *type) {
            int len = part.length();

            if (len < 1 || len > 63)
                throw Exception(Error::Parse, "%1 must consist of at least 1 and at most 63 characters (found %2 characters)").arg(qL1S(type)).arg(len);

            for (int pos = 0; pos < len; ++pos) {
                ushort ch = part.at(pos).unicode();
                bool isFirst = (pos == 0);
                bool isLast  = (pos == (len - 1));
                bool isDash  = (ch == '-');
                bool isDigit = (ch >= '0' && ch <= '9');
                bool isLower = (ch >= 'a' && ch <= 'z');

                if ((isFirst || isLast || !isDash) && !isDigit && !isLower)
                    throw Exception(Error::Parse, "%1 must consists of only the characters '0-9', 'a-z', and '-' (which cannot be the first or last character)").arg(qL1S(type));
            }
        };

        if (isAliasName) {
            QString aliasTag = parts.last();
            int sepPos = aliasTag.indexOf(qL1C('@'));
            if (sepPos < 0 || sepPos == (aliasTag.size() - 1))
                throw Exception(Error::Parse, "missing alias-id tag");

            parts.removeLast();
            parts << aliasTag.left(sepPos);
            parts << aliasTag.mid(sepPos + 1);
        }

        for (int i = 0; i < parts.size(); ++i) {
            const QString &part = parts.at(i);
            partCheck(part, isAliasName && (i == (part.size() - 1)) ? "tag name" : "parts (subdomains)");
        }

        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

int versionCompare(const QString &version1, const QString &version2)
{
    int pos1 = 0;
    int pos2 = 0;
    int len1 = version1.length();
    int len2 = version2.length();

    forever {
        if (pos1 == len1 && pos2 == len2)
            return 0;
       else if (pos1 >= len1)
            return -1;
        else if (pos2 >= len2)
            return +1;

        QString part1 = version1.mid(pos1++, 1);
        QString part2 = version2.mid(pos2++, 1);

        bool isDigit1 = part1.at(0).isDigit();
        bool isDigit2 = part2.at(0).isDigit();

        if (!isDigit1 || !isDigit2) {
            int cmp = part1.compare(part2);
            if (cmp)
                return (cmp > 0) ? 1 : -1;
        } else {
            while ((pos1 < len1) && version1.at(pos1).isDigit())
                part1.append(version1.at(pos1++));
            while ((pos2 < len2) && version2.at(pos2).isDigit())
                part2.append(version2.at(pos2++));

            int num1 = part1.toInt();
            int num2 = part2.toInt();

            if (num1 != num2)
                return (num1 > num2) ? 1 : -1;
        }
    }
}


TemporaryDir::TemporaryDir()
    : QTemporaryDir()
{}

TemporaryDir::TemporaryDir(const QString &templateName)
    : QTemporaryDir(templateName)
{}

TemporaryDir::~TemporaryDir()
{
    recursiveOperation(path(), SafeRemove());
}

bool SafeRemove::operator()(const QString &path, RecursiveOperationType type)
{
   static const QFileDevice::Permissions fullAccess =
           QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
           QFileDevice::ReadUser  | QFileDevice::WriteUser  | QFileDevice::ExeUser  |
           QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
           QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

   switch (type) {
   case RecursiveOperationType::EnterDirectory:
       return QFile::setPermissions(path, fullAccess);

   case RecursiveOperationType::LeaveDirectory: {
        // QDir cannot delete the directory it is pointing to
       QDir dir(path);
       QString dirName = dir.dirName();
       return dir.cdUp() && dir.rmdir(dirName);
   }
   case RecursiveOperationType::File:
       return QFile::remove(path);
   }
   return false;
}

#if defined(Q_OS_UNIX)

SetOwnerAndPermissions::SetOwnerAndPermissions(uid_t user, gid_t group, mode_t permissions)
    : m_user(user)
    , m_group(group)
    , m_permissions(permissions)
{ }


bool SetOwnerAndPermissions::operator()(const QString &path, RecursiveOperationType type)
{
    if (type == RecursiveOperationType::EnterDirectory)
        return true;

    const QByteArray localPath = path.toLocal8Bit();
    mode_t mode = m_permissions;

    if (type == RecursiveOperationType::LeaveDirectory) {
        // set the x bit for directories, but only where it makes sense
        if (mode & 06)
            mode |= 01;
        if (mode & 060)
            mode |= 010;
        if (mode & 0600)
            mode |= 0100;
    }

    return ((chmod(localPath, mode) == 0) && (chown(localPath, m_user, m_group) == 0));
}

#endif // Q_OS_UNIX


#if defined(Q_OS_ANDROID)

#include <QtAndroidExtras>

QString findOnSDCard(const QString &file)
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    QAndroidJniObject dirs = activity.callObjectMethod("getExternalFilesDirs", "(Ljava/lang/String;)[Ljava/io/File;", NULL);

    if (dirs.isValid()) {
        QAndroidJniEnvironment env;
        for (int i = 0; i < env->GetArrayLength(dirs.object<jarray>()); ++i) {
            QAndroidJniObject dir = env->GetObjectArrayElement(dirs.object<jobjectArray>(), i);

            QString path = QDir(dir.toString()).absoluteFilePath(file);

            if (QFile(path).open(QIODevice::ReadOnly)) {
                qWarning(" ##### found %s on external dir [%d] path: %s", qPrintable(file), i, qPrintable(dir.toString()));
                return path;
            }
        }
    }
    return QString();
}

#endif


