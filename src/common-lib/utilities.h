// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtCore/QVector>
#include <QtCore/QPair>
#include <QtCore/QByteArray>
#include <QtCore/QVariant>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtCore/QResource>
#include <QtCore/QLibrary>

#include <QtAppManCommon/global.h>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QDir)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

int timeoutFactor();

using YamlFormat = QPair<QString, int>;

YamlFormat checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                           const QVector<YamlFormat> &formatTypesAndVersions) noexcept(false);

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
    return path.startsWith(u":") ? QUrl(QStringLiteral("qrc") + path)
                                 : QUrl::fromUserInput(path, baseDir, QUrl::AssumeLocalFile);
}

// Used in {Package,Application,Intent}::name()
QString translateFromMap(const QMap<QString, QString> &languageToName, const QString &defaultName = {});

inline QString urlToLocalFilePath(const QUrl &url)
{
    if (url.isLocalFile())
        return url.toLocalFile();
    else if (url.scheme() == u"qrc")
        return u':' + url.path();
    return { };
}

inline QString toAbsoluteFilePath(const QString &path, const QString &baseDir = QDir::currentPath())
{
    return path.startsWith(u"qrc:") ? path.mid(3) : QDir(baseDir).absoluteFilePath(path);
}

/*! \internal
    Recursively merge the second QVariantMap into the first one
*/
void recursiveMergeVariantMap(QVariantMap &into, const QVariantMap &from);

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

QVector<QObject *> loadPlugins_helper(const char *type, const QStringList &files, const char *iid) noexcept(false);

template <typename T>
QVector<T *> loadPlugins(const char *type, const QStringList &files) noexcept(false)
{
    QVector<T *> result;
    const auto plugins = loadPlugins_helper(type, files, qobject_interface_iid<T *>());
    for (auto p : plugins)
        result << qobject_cast<T *>(p);
    return result;
}

// Load a Qt resource, either in the form of a resource file or a plugin
void loadResource(const QString &resource) noexcept(false);

// Qt6 removed v_cast, but the "replacement" QVariant::Private::get is const only
template <typename T> T *qt6_v_cast(QVariant::Private *vp)
{
    return static_cast<T *>(const_cast<void *>(vp->storage()));
}

// close all valid file descriptors and then clear the (non-const) vector
void closeAndClearFileDescriptors(QVector<int> &fdList);

// make sure that the given id can be used as a filename
void validateIdForFilesystemUsage(const QString &id) noexcept(false);

QT_END_NAMESPACE_AM


#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)

QT_BEGIN_NAMESPACE

void putTimeUnit(QDebug &dbg, qint64 num, qint64 den);

template <typename Rep, typename Period>
QDebug &operator<<(QDebug &dbg, std::chrono::duration<Rep, Period> duration)
{
    dbg << duration.count();
    putTimeUnit(dbg, Period::num, Period::den);
    return dbg.maybeSpace();
}

QT_END_NAMESPACE

#endif // QT_VERSION < QT_VERSION_CHECK(6, 6, 0)

#endif // UTILITIES_H
