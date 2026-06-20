// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include "filesystemmountwatcher.h"
#include "utilities.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_FileSystemMountWatcher : public QObject
{
    Q_OBJECT

public:
    tst_FileSystemMountWatcher(QObject *parent = nullptr);

private Q_SLOTS:
    void initTestCase();
    void currentMountPoints();
    void watch();
    void addRemoveInvalid();

private:
    // rewrite the fake mtab file with the given <mountPoint, device> entries
    bool writeMtab(const QMultiMap<QString, QString> &mounts);

    QTemporaryDir m_tmp;
    QString m_mtabFile;
    int m_spyTimeout = 5000 * timeoutFactor();
};

tst_FileSystemMountWatcher::tst_FileSystemMountWatcher(QObject *)
{ }

bool tst_FileSystemMountWatcher::writeMtab(const QMultiMap<QString, QString> &mounts)
{
    // mtab/fstab line format: <device> <mountpoint> <fstype> <options> <dump> <pass>
    QByteArray content;
    for (auto it = mounts.cbegin(); it != mounts.cend(); ++it)
        content += (it.value() + u' ' + it.key() + u" ext4 rw 0 0\n"_s).toLocal8Bit();

    QFile f(m_mtabFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(content) == content.size();
}

void tst_FileSystemMountWatcher::initTestCase()
{
    QVERIFY(m_tmp.isValid());
    m_mtabFile = QDir(m_tmp.path()).absoluteFilePath(u"mtab"_s);

    // start with a single entry, so the watched file exists before any watcher is created
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s } }));

    // redirect the watcher away from /proc/self/mounts onto our controllable file. This has to
    // happen before the first FileSystemMountWatcher is ever constructed.
    QVERIFY(FileSystemMountWatcher::setMountTabFileForTesting(m_mtabFile));
}

void tst_FileSystemMountWatcher::currentMountPoints()
{
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s },
                        { u"/boot"_s, u"/dev/sda1"_s } }));

    // currentMountPoints() reads our fake mtab and returns mountPoint -> device
    const auto mounts = FileSystemMountWatcher::currentMountPoints();
    QCOMPARE(mounts.value(u"/"_s), u"/dev/root"_s);
    QCOMPARE(mounts.value(u"/boot"_s), u"/dev/sda1"_s);
    QVERIFY(!mounts.contains(u"/not-mounted"_s));
}

void tst_FileSystemMountWatcher::watch()
{
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s } }));

    FileSystemMountWatcher watcher;
    QSignalSpy spy(&watcher, &FileSystemMountWatcher::mountChanged);

    // we only care about /data being (un)mounted
    watcher.addMountPoint(u"/data"_s);

    // mount /data -> we get notified with the backing device
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s },
                        { u"/data"_s, u"/dev/sdb1"_s } }));
    QVERIFY(spy.wait(m_spyTimeout));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"/data"_s);
    QCOMPARE(spy.at(0).at(1).toString(), u"/dev/sdb1"_s);

    // a change to an unwatched mount point must not notify us
    spy.clear();
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s },
                        { u"/data"_s, u"/dev/sdb1"_s },
                        { u"/boot"_s, u"/dev/sda1"_s } }));
    QVERIFY(!spy.wait(1000 * timeoutFactor()));

    // unmount /data -> notified again, this time with an empty device
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s },
                        { u"/boot"_s, u"/dev/sda1"_s } }));
    QVERIFY(spy.wait(m_spyTimeout));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"/data"_s);
    QCOMPARE(spy.at(0).at(1).toString(), QString());

    // after removing the mount point, further changes are ignored
    watcher.removeMountPoint(u"/data"_s);
    spy.clear();
    QVERIFY(writeMtab({ { u"/"_s, u"/dev/root"_s },
                        { u"/data"_s, u"/dev/sdb1"_s } }));
    QVERIFY(!spy.wait(1000 * timeoutFactor()));
}

void tst_FileSystemMountWatcher::addRemoveInvalid()
{
    auto *watcher = new FileSystemMountWatcher;

    // empty mount points are rejected (no crash, no effect)
    watcher->addMountPoint(QString());
    watcher->removeMountPoint(QString());

    // removing something that was never added is harmless
    watcher->removeMountPoint(u"/never-added"_s);

    // setMountTabFileForTesting now fails, because the shared private object already exists
    QVERIFY(!FileSystemMountWatcher::setMountTabFileForTesting(m_mtabFile));

    // destroying a watcher that still has a registered mount point must detach it cleanly
    watcher->addMountPoint(u"/data"_s);
    delete watcher;
}

QTEST_GUILESS_MAIN(tst_FileSystemMountWatcher)

#include "tst_filesystemmountwatcher.moc"
