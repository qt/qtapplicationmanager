/****************************************************************************
**
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

#pragma once

#include <QVector>
#include <QByteArray>
#include <QMultiMap>
#include <QVariant>

#include <QtAppManCommon/global.h>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QDir)

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

/*! \internal
    Recursively merge the second QVariantMap into the first one
*/
void recursiveMergeVariantMap(QVariantMap &into, const QVariantMap &from);

QMultiMap<QString, QString> mountedDirectories();

enum class RecursiveOperationType
{
    EnterDirectory,
    LeaveDirectory,
    File
};

/*! \internal

    Recursively iterates over the file-system tree at \a path and calls the
    functor \a operation for each entry.

    \c path is always the file-path to the current entry. For files, \a
    operation will only be called once (\c{type == File}), whereas for
    directories, the functor will be triggered twice: once when entering the
    directory (\c{type == EnterDirectory}) and once when all sub-directories
    and files have been processed (\c{type == LeaveDirectory}).
 */
bool recursiveOperation(const QString &path, const std::function<bool(const QString &, RecursiveOperationType)> &operation);

// convenience
bool recursiveOperation(const QByteArray &path, const std::function<bool(const QString &, RecursiveOperationType)> &operation);
bool recursiveOperation(const QDir &path, const std::function<bool(const QString &, RecursiveOperationType)> &operation);

// makes files and directories writable, then deletes them
bool safeRemove(const QString &path, RecursiveOperationType type);

void getOutputInformation(bool *ansiColorSupport, bool *runningInCreator, int *consoleWidth);

qint64 getParentPid(qint64 pid);

QVector<QObject *> loadPlugins_helper(const char *type, const QStringList &files, const char *iid) Q_DECL_NOEXCEPT_EXPR(false);

template <typename T>
QVector<T *> loadPlugins(const char *type, const QStringList &files) Q_DECL_NOEXCEPT_EXPR(false)
{
    QVector<T *> result;
    auto plugins = loadPlugins_helper(type, files, qobject_interface_iid<T *>());
    for (auto p : plugins)
        result << qobject_cast<T *>(p);
    return result;
}

QT_END_NAMESPACE_AM
