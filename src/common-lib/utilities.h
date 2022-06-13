/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QVector>
#include <QPair>
#include <QByteArray>
#include <QMultiMap>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QDir>
#include <QResource>
#include <QLibrary>

#include <QtAppManCommon/global.h>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QDir)

QT_BEGIN_NAMESPACE_AM

int timeoutFactor();

using YamlFormat = QPair<QString, int>;

YamlFormat checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                           const QVector<YamlFormat> &formatTypesAndVersions) Q_DECL_NOEXCEPT_EXPR(false);

/*! \internal
    Convenience function that makes it easy to accept a plain string where
    a stringlist is required - this is useful when parsing YAML config files
*/
inline QStringList variantToStringList(const QVariant &v)
{
    return (v.metaType() == QMetaType::fromType<QString>()) ? QStringList(v.toString())
                                                            : v.toStringList();
}

// Translate between QFile and QUrl (resource) representations.
// For some weird reason, QFile cannot cope with "qrc:" and QUrl cannot cope with ":".
inline QUrl filePathToUrl(const QString &path, const QString &baseDir)
{
    return path.startsWith(qSL(":")) ? QUrl(qSL("qrc") + path)
                                     : QUrl::fromUserInput(path, baseDir, QUrl::AssumeLocalFile);
}

inline QString urlToLocalFilePath(const QUrl &url)
{
    if (url.isLocalFile())
        return url.toLocalFile();
    else if (url.scheme() == qSL("qrc"))
        return qL1C(':') + url.path();
    return QString();
}

inline QString toAbsoluteFilePath(const QString &path, const QString &baseDir = QDir::currentPath())
{
    return path.startsWith(qSL("qrc:")) ? path.mid(3) : QDir(baseDir).absoluteFilePath(path);
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

// Load a Qt resource, either in the form of a resource file or a plugin
inline bool loadResource(const QString &resource)
{
    return QLibrary::isLibrary(resource)
           ? (QLibrary(QDir().absoluteFilePath(resource)).load() || QResource::registerResource(resource))
           : (QResource::registerResource(resource) || QLibrary(QDir().absoluteFilePath(resource)).load());
}

// Qt6 removed v_cast, but the "replacement" QVariant::Private::get is const only
template <typename T> T *qt6_v_cast(QVariant::Private *vp)
{
    return static_cast<T *>(const_cast<void *>(vp->storage()));
}

QT_END_NAMESPACE_AM
