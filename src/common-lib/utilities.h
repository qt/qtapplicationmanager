/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QDirIterator>
#include <QByteArray>
#include <QMultiMap>

#include <stdlib.h>


bool isValidDnsName(const QString &rnds, bool isAliasName = false, QString *errorString = 0);
int versionCompare(const QString &version1, const QString &version2);

bool diskUsage(const QString &path, quint64 *bytesTotal, quint64 *bytesFree);
QMultiMap<QString, QString> mountedDirectories();

/*! \internal

    The standard QTemporaryDir destructor cannot cope with read-only sub-directories.
 */
class TemporaryDir : public QTemporaryDir
{
public:
    TemporaryDir();
    explicit TemporaryDir(const QString &templateName);
    ~TemporaryDir();

private:
    Q_DISABLE_COPY(TemporaryDir)
};

enum class RecursiveOperationType
{
    EnterDirectory,
    LeaveDirectory,
    File
};

/*! \internal

    Recursively iterates over the file-system tree at \a path and calls the
    functor \a operation for each each entry as
    \c{operator(const QString &path, RecursiveOperationType type)}

    \c path is always the file-path to the current entry. For files, \a
    operation will only be called once (\c{type == File}), whereas for
    directories, the functor will be triggered twice: once when entering the
    directory (\c{type == EnterDirectory}) and once when all sub-directories
    and files have been processed (\c{type == LeaveDirectory}).
 */
template <typename OP> static bool recursiveOperation(const QString &path, OP operation)
{
    QFileInfo pathInfo(path);

    if (pathInfo.isDir()) {
        if (!operation(path, RecursiveOperationType::EnterDirectory))
            return false;

        QDirIterator dit(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (dit.hasNext()) {
            dit.next();
            QFileInfo ditInfo = dit.fileInfo();

            if (ditInfo.isDir()) {
                if (!recursiveOperation(ditInfo.filePath(), operation))
                    return false;
            } else {
                if (!operation(ditInfo.filePath(), RecursiveOperationType::File))
                    return false;
            }
        }
        return operation(path, RecursiveOperationType::LeaveDirectory);
    } else {
        return operation(path, RecursiveOperationType::File);
    }
}

template <typename OP> static bool recursiveOperation(const QByteArray &path, OP operation)
{
    return recursiveOperation(QString::fromLocal8Bit(path), operation);
}

template <typename OP> static bool recursiveOperation(const QDir &path, OP operation)
{
    return recursiveOperation(path.absolutePath(), operation);
}

// makes files and directories writable, then deletes them
class SafeRemove
{
public:
    bool operator()(const QString &path, RecursiveOperationType type);
};

// changes owner and permissions (Unix only)
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/stat.h>

class SetOwnerAndPermissions
{
public:
    SetOwnerAndPermissions(uid_t user, gid_t group, mode_t permissions);
    bool operator()(const QString &path, RecursiveOperationType type);

private:
    uid_t m_user;
    gid_t m_group;
    mode_t m_permissions;
};
#endif

#if defined(Q_OS_ANDROID)
QString findOnSDCard(const QString &file);
#endif
