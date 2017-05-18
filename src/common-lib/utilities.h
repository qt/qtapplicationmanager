/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QDirIterator>
#include <QByteArray>
#include <QMultiMap>
#include <QPluginLoader>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/exception.h>

#include <stdlib.h>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/stat.h>
#endif


QT_BEGIN_NAMESPACE_AM

int timeoutFactor();

void checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                     const QVector<QByteArray> &formatTypes, int formatVersion) Q_DECL_NOEXCEPT_EXPR(false);

/*! \internal
    Convenience function that makes it easy to accept a plain string where
    a stringlist is required - this is useful when parsing YAML config files
*/
inline QStringList variantToStringList(const QVariant &v)
{
    return (v.type() == QVariant::String) ? QStringList(v.toString())
                                          : v.toStringList();
}

bool diskUsage(const QString &path, quint64 *bytesTotal, quint64 *bytesFree);
QMultiMap<QString, QString> mountedDirectories();

enum class RecursiveOperationType
{
    EnterDirectory,
    LeaveDirectory,
    File
};

/*! \internal

    Recursively iterates over the file-system tree at \a path and calls the
    functor \a operation for each entry as
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

void getOutputInformation(bool *ansiColorSupport, bool *runningInCreator, int *consoleWidth);

qint64 getParentPid(qint64 pid);

template <typename T>
QVector<T *> loadPlugins(const char *type, const QStringList &files) Q_DECL_NOEXCEPT_EXPR(false)
{
    QVector<T *> interfaces;
    const char *iid = qobject_interface_iid<T>();

    for (const QString &pluginFilePath : files) {
        QPluginLoader pluginLoader(pluginFilePath);
        if (Q_UNLIKELY(!pluginLoader.load())) {
            throw Exception("could not load %1 plugin %2: %3")
                    .arg(type).arg(pluginFilePath, pluginLoader.errorString());
        }
        QScopedPointer<T >iface(qobject_cast<T *>(pluginLoader.instance()));

        if (Q_UNLIKELY(!iface)) {
            throw Exception("could not get an instance of '%1' from the %2 plugin %3")
                    .arg(iid).arg(type).arg(pluginFilePath);
        }
        interfaces << iface.take();
    }
    return interfaces;
}

QT_END_NAMESPACE_AM
