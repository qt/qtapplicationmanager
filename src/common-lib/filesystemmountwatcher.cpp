/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include <QSocketNotifier>
#include <QFileSystemWatcher>

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/mount.h>
#elif defined(Q_OS_LINUX)
#  include <mntent.h>
#endif
#include <qplatformdefs.h>

#include "filesystemmountwatcher.h"

QT_BEGIN_NAMESPACE_AM


class FileSystemMountWatcherPrivate
{
public:
    FileSystemMountWatcherPrivate()
    {
#if defined(Q_OS_LINUX)
        if (s_mountTabFile.isEmpty()) {
            s_mountTabFile = "/proc/self/mounts";;

            m_procMountsFd = QT_OPEN(s_mountTabFile.constData(), O_RDONLY);
            if (m_procMountsFd >= 0) {
                m_procMountsNotifier = new QSocketNotifier(m_procMountsFd, QSocketNotifier::Exception);
                QObject::connect(m_procMountsNotifier, &QSocketNotifier::activated,
                                 [this]() { mountsChanged(); });
            }
        } else {
            m_autoTestMountTabWatcher = new QFileSystemWatcher({ QString::fromLocal8Bit(s_mountTabFile) });
            QObject::connect(m_autoTestMountTabWatcher, &QFileSystemWatcher::fileChanged,
                             [this]() { mountsChanged(); });
        }
#endif
    }

    ~FileSystemMountWatcherPrivate()
    {
        delete m_autoTestMountTabWatcher;
        delete m_procMountsNotifier;
        if (m_procMountsFd >= 0)
            QT_CLOSE(m_procMountsFd);
    }

    bool add(FileSystemMountWatcher *watcher, const QString &mountPoint)
    {
        if ((!m_procMountsNotifier && !m_autoTestMountTabWatcher) || !watcher || mountPoint.isEmpty())
            return false;

        m_mountPoints.insert(mountPoint, watcher);

        if (m_mountPoints.size() == 1)
            m_mounts = currentMountPoints(); // we need to know from where we started
        return true;
    }

    bool remove(FileSystemMountWatcher *watcher, const QString &mountPoint)
    {
        if ((!m_procMountsNotifier && !m_autoTestMountTabWatcher) || !watcher || mountPoint.isEmpty())
            return false;

        m_mountPoints.remove(mountPoint, watcher);
        return true;
    }

    void attach()
    {
        m_ref.ref();
    }

    bool detach(FileSystemMountWatcher *watcher)
    {
        for (auto it = m_mountPoints.begin(); it != m_mountPoints.end(); ) {
            if (it.value() == watcher)
                it = m_mountPoints.erase(it);
            else
                ++it;
        }
        return !m_ref.deref();
    }

    static QMultiMap<QString, QString> currentMountPoints()
    {
        QMultiMap<QString, QString> result;
#if defined(Q_OS_WIN)
        return result; // no mounts on Windows

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        struct statfs *sfs = nullptr;
        int count = getmntinfo(&sfs, MNT_NOWAIT);

        for (int i = 0; i < count; ++i, ++sfs) {
            result.insert(QString::fromLocal8Bit(sfs->f_mntonname), QString::fromLocal8Bit(sfs->f_mntfromname));
        }
#else
        FILE *pm = fopen(s_mountTabFile.constData(), "r");
        if (!pm)
            return result;

#  if defined(Q_OS_ANDROID)
        while (struct mntent *mntPtr = getmntent(pm)) {
            result.insert(QString::fromLocal8Bit(mntPtr->mnt_dir),
                          QString::fromLocal8Bit(mntPtr->mnt_fsname));
        }
#  else
        static const int pathMax = static_cast<int>(pathconf("/", _PC_PATH_MAX)) * 2 + 1024;  // quite big, but better be safe than sorry
        QScopedArrayPointer<char> strBuf(new char[pathMax]);
        struct mntent mntBuf;

        while (getmntent_r(pm, &mntBuf, strBuf.data(), pathMax - 1)) {
            result.insert(QString::fromLocal8Bit(mntBuf.mnt_dir),
                          QString::fromLocal8Bit(mntBuf.mnt_fsname));
        }
#  endif
        fclose(pm);
#endif

        return result;
    }

    void mountsChanged()
    {
        if (m_mountPoints.isEmpty())
            return;

        auto newMounts = currentMountPoints();

        for (auto it = m_mountPoints.cbegin(); it != m_mountPoints.cend(); ++it) {
            const QString mp = it.key();
            bool isMounted = newMounts.contains(mp);
            bool wasMounted = m_mounts.contains(mp);

            if (isMounted != wasMounted)
                emit it.value()->mountChanged(mp, newMounts.value(mp));
        }

        m_mounts = newMounts;
    }

private:
    QAtomicInt m_ref;

    static QByteArray s_mountTabFile;

    int m_procMountsFd = -1;
    QSocketNotifier *m_procMountsNotifier = nullptr;
    QFileSystemWatcher *m_autoTestMountTabWatcher = nullptr;
    QMultiMap<QString, FileSystemMountWatcher *> m_mountPoints;
    QMultiMap<QString, QString> m_mounts;

    friend class FileSystemMountWatcher;
};

QByteArray FileSystemMountWatcherPrivate::s_mountTabFile;


FileSystemMountWatcherPrivate *FileSystemMountWatcher::d = nullptr;

FileSystemMountWatcher::FileSystemMountWatcher(QObject *parent)
    : QObject(parent)
{
    if (!d)
        d = new FileSystemMountWatcherPrivate();
    d->attach();
}

FileSystemMountWatcher::~FileSystemMountWatcher()
{
    if (d->detach(this)) {
        delete d;
        d = nullptr;
    }
}

QMultiMap<QString, QString> FileSystemMountWatcher::currentMountPoints()
{
    // if we have an active cache, then use it
    if (d && d->m_procMountsNotifier)
        return d->m_mounts;
    return FileSystemMountWatcherPrivate::currentMountPoints();
}

void FileSystemMountWatcher::addMountPoint(const QString &directory)
{
    d->add(this, directory);
}

void FileSystemMountWatcher::removeMountPoint(const QString &directory)
{
    d->remove(this, directory);
}

bool FileSystemMountWatcher::setMountTabFileForTesting(const QString &mtabFile)
{
    if (d)
        return false; // too late
#if defined(Q_OS_LINUX)
    FileSystemMountWatcherPrivate::s_mountTabFile = mtabFile.toLocal8Bit();
    return true;
#else
    Q_UNUSED(mtabFile)
    return false;
#endif
}

QT_END_NAMESPACE_AM
