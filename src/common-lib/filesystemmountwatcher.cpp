// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
            s_mountTabFile = "/proc/self/mounts";

            m_procMountsFd = QT_OPEN(s_mountTabFile.constData(), O_RDONLY);
            if (m_procMountsFd >= 0) {
                m_procMountsNotifier = new QSocketNotifier(m_procMountsFd, QSocketNotifier::Exception);
                QObject::connect(m_procMountsNotifier, &QSocketNotifier::activated,
                                 m_procMountsNotifier, [this]() { mountsChanged(); });
            }
        } else {
            m_autoTestMountTabWatcher = new QFileSystemWatcher({ QString::fromLocal8Bit(s_mountTabFile) });
            QObject::connect(m_autoTestMountTabWatcher, &QFileSystemWatcher::fileChanged,
                             m_autoTestMountTabWatcher, [this]() { mountsChanged(); });
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
        m_mountPoints.removeIf([=](decltype(m_mountPoints)::iterator it) {
            return it.value() == watcher;
        });
        return !m_ref.deref();
    }

    static QMultiMap<QString, QString> currentMountPoints()
    {
        QMultiMap<QString, QString> result;
#if defined(Q_OS_WIN) || defined (Q_OS_QNX)
        return result; // no mounts on Windows, not supported on QNX

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
        static const size_t pathMax = static_cast<size_t>(pathconf("/", _PC_PATH_MAX)) * 2 + 1024;  // quite big, but better be safe than sorry
        QScopedArrayPointer<char> strBuf(new char[pathMax]);
        struct mntent mntBuf;

        while (getmntent_r(pm, &mntBuf, strBuf.data(), int(pathMax) - 1)) {
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
    if (d->m_procMountsNotifier)
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

#include "moc_filesystemmountwatcher.cpp"
