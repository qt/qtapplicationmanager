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

#include "scopeutilities.h"
#include "applicationinstaller_p.h"
#include "utilities.h"


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


ScopedFileCreator::ScopedFileCreator()
{ }

bool ScopedFileCreator::create(const QString &path, bool removeExisting)
{
    m_file.setFileName(path);
    QFileInfo fi(m_file);

    if (fi.exists() && fi.isFile()) {
        if (!removeExisting)
            return m_created = true;
        else if (!removeRecursiveHelper(fi.absoluteFilePath()))
            return false;
    }
    return m_file.open(QFile::ReadWrite | QFile::Truncate);
}

bool ScopedFileCreator::take()
{
    if (m_created && !m_taken) {
        m_file.close();
        m_taken = true;
    }
    return m_taken;
}

ScopedFileCreator::~ScopedFileCreator()
{
    m_file.close();
    if (m_created && !m_taken)
        removeRecursiveHelper(m_file.fileName());
}

QFile *ScopedFileCreator::file()
{
    return &m_file;
}


ScopedLoopbackCreator::ScopedLoopbackCreator()
{ }

bool ScopedLoopbackCreator::create(const QString &imagePath)
{
    m_loopDevice = SudoClient::instance()->attachLoopback(imagePath);
    return !m_loopDevice.isEmpty();
}

bool ScopedLoopbackCreator::take()
{
    if (!m_loopDevice.isEmpty() && !m_taken)
        m_taken = true;
    return m_taken;
}

bool ScopedLoopbackCreator::destroy()
{
    if (!m_taken) {
        m_taken = m_loopDevice.isEmpty()
                || SudoClient::instance()->detachLoopback(m_loopDevice);

        if (!m_taken) {
            qCCritical(LogInstaller).nospace() << "could not remove the loopback device " << m_loopDevice
                                               << ": " << errorString();
        }
    }
    return m_taken;
}

ScopedLoopbackCreator::~ScopedLoopbackCreator()
{
    destroy();
}

QString ScopedLoopbackCreator::device() const
{
    return m_loopDevice;
}

QString ScopedLoopbackCreator::errorString() const
{
    return SudoClient::instance()->lastError();
}


ScopedMounter::ScopedMounter()
{ }

bool ScopedMounter::mount(const QString &devicePath, const QString &mountPoint, bool readWrite)
{
    m_devicePath = devicePath;
    m_mountPoint = mountPoint;
    return m_mounted = SudoClient::instance()->mount(m_devicePath, m_mountPoint, readWrite);
}

bool ScopedMounter::unmount()
{
    if (!m_taken) {
        m_taken = !m_mounted
                || SudoClient::instance()->unmount(m_mountPoint)
                || SudoClient::instance()->unmount(m_mountPoint, true /*force*/);

        //qWarning() << "SCOPEDUMOUNTER unmounted" << m_devicePath << m_mountPoint;

        if (!m_taken) {
            qCCritical(LogInstaller).nospace() << "could not unmount device " << m_devicePath
                                               << " at " << m_mountPoint
                                               << ": " << errorString();
        }
    }
    return m_taken;
}

bool ScopedMounter::take()
{
    if (m_mounted && !m_taken)
        m_taken = true;
    return m_taken;
}

ScopedMounter::~ScopedMounter()
{
    unmount();
}

QString ScopedMounter::errorString() const
{
    return SudoClient::instance()->lastError();
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
        if (toInfo.exists() && !recursiveOperation(toInfo.absoluteFilePath(), SafeRemove()))
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
    m_basePath = fi.absolutePath();
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


