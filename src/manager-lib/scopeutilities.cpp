// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "logging.h"
#include "scopeutilities.h"
#include "packagemanager_p.h"
#include "utilities.h"

QT_BEGIN_NAMESPACE_AM

ScopedDirectoryCreator::ScopedDirectoryCreator()
{ }

bool ScopedDirectoryCreator::create(const QString &path, bool removeExisting)
{
    m_path = path;
    QFileInfo fi(m_path);

    if (fi.exists() && fi.isDir()) {
        if (!removeExisting)
            return m_created = true;
        else if (!removeRecursiveHelper(m_path))
            return false;
    }
    //qWarning() << "CREATE" << path << fi.absolutePath() << fi.fileName();
    return m_created = QDir::root().mkpath(path);
}

bool ScopedDirectoryCreator::take()
{
    if (m_created && !m_taken)
        m_taken = true;
    return m_taken;
}

bool ScopedDirectoryCreator::destroy()
{
    if (!m_taken) {
        if (m_created )
            removeRecursiveHelper(m_path);
        m_taken = true;
    }
    return m_taken;
}

ScopedDirectoryCreator::~ScopedDirectoryCreator()
{
    destroy();
}

QDir ScopedDirectoryCreator::dir()
{
    return QDir(m_path);
}

ScopedRenamer::ScopedRenamer()
{ }

bool ScopedRenamer::internalRename(const QDir &dir, const QString &from, const QString &to)
{
    QFileInfo fromInfo(dir.absoluteFilePath(from));
    QFileInfo toInfo(dir.absoluteFilePath(to));

    // POSIX cannot atomically rename directories, if the destination directory exists
    // and is non-empty. We need to delete it first.
    // Windows on the other hand cannot do anything atomically.
#ifdef Q_OS_UNIX
    if (fromInfo.isDir()) {
#else
    Q_UNUSED(fromInfo)

    if (true) {
#endif
        if (toInfo.exists() && !recursiveOperation(toInfo.absoluteFilePath(), safeRemove))
            return false;
    }
#ifdef Q_OS_UNIX
    return (::rename(fromInfo.absoluteFilePath().toLocal8Bit(), toInfo.absoluteFilePath().toLocal8Bit()) == 0);
#else
    return QDir(dir).rename(from, to);
#endif
}

bool ScopedRenamer::rename(const QString &baseName, ScopedRenamer::Modes modes)
{
    QFileInfo fi(baseName);
    m_basePath.setPath(fi.absolutePath());
    m_name = fi.fileName();
    m_requested = modes;

    // convenience
    bool backupRequired = (modes & NameToNameMinus);
    bool backupDone = false;

    if (m_requested & NameToNameMinus) {
        backupDone = internalRename(m_basePath, m_name, m_name + qL1C('-'));
        if (backupDone)
            m_done |= NameToNameMinus;
    }
    if (m_requested & NamePlusToName) {
        // only try if no backup required, or it worked
        if (!backupRequired || backupDone) {
            if (internalRename(m_basePath, m_name + qL1C('+'), m_name)) {
                m_done |= NamePlusToName;
            }
            else if (backupDone && !undoRename()) {
                qCCritical(LogSystem) << QString::fromLatin1("failed to rename '%1+' to '%1', but also failed to rename backup '%1-' back to '%1' (in directory %2)")
                                         .arg(m_name, m_basePath.absolutePath());
            }
        }
    }
    return m_requested == m_done;
}

bool ScopedRenamer::rename(const QDir &baseName, ScopedRenamer::Modes modes)
{
    return rename(baseName.absolutePath(), modes);
}


bool ScopedRenamer::take()
{
    if (!m_taken)
        m_taken = true;
    return m_taken;
}

bool ScopedRenamer::undoRename()
{
    if (!m_taken) {
        if (interalUndoRename()) {
            m_taken = true;
        } else {
            if (m_done & NamePlusToName) {
                qCCritical(LogSystem) << QString::fromLatin1("failed to undo rename from '%1+' to '%1' (in directory %2)")
                                         .arg(m_name, m_basePath.absolutePath());
            }
            if (m_done & NameToNameMinus) {
                qCCritical(LogSystem) << QString::fromLatin1("failed to undo rename from '%1' to '%1-' (in directory %2)")
                                         .arg(m_name, m_basePath.absolutePath());
            }
        }
    }
    return m_taken;
}

bool ScopedRenamer::interalUndoRename()
{
    if (m_done & NamePlusToName) {
        if (internalRename(m_basePath, m_name, m_name + qL1C('+')))
            m_done &= ~NamePlusToName;
    }
    if (m_done & NameToNameMinus) {
        if (internalRename(m_basePath, m_name + qL1C('-'), m_name))
            m_done &= ~NameToNameMinus;
    }

    return (m_done == 0);
}

ScopedRenamer::~ScopedRenamer()
{
    undoRename();
}

bool ScopedRenamer::isRenamed() const
{
    return m_requested && (m_requested == m_done);
}

QString ScopedRenamer::baseName() const
{
    return m_basePath.absoluteFilePath(m_name);
}

QT_END_NAMESPACE_AM
