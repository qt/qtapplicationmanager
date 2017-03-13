/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <qplatformdefs.h>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include <linux/loop.h>
#include <sys/ioctl.h>

#include "utilities.h"
#include "sudo.h"

#include "../sudo-cleanup.h"

QT_USE_NAMESPACE_AM

static int timeoutFactor = 1; // useful to increase timeouts when running in valgrind
static int processTimeout = 3000;

static bool startedSudoServer = false;
static QString sudoServerError;

#define LOSETUP "/sbin/losetup"
#define MOUNT   "/bin/mount"
#define UMOUNT  "/bin/umount"
#define BLKID   "/sbin/blkid"

// sudo RAII style
class ScopedRootPrivileges
{
public:
    ScopedRootPrivileges()
    {
        m_uid = getuid();
        m_gid = getgid();
        if (setresuid(0, 0, 0) || setresgid(0, 0, 0))
            QFAIL("cannot re-gain root privileges");
    }
    ~ScopedRootPrivileges()
    {
        if (setresgid(m_gid, m_gid, 0) || setresuid(m_uid, m_uid, 0))
            QFAIL("cannot drop root privileges");
    }
private:
    uid_t m_uid;
    gid_t m_gid;
};

#define SUB_QVERIFY2(statement, description) \
do {\
    if (statement) {\
        if (!QTest::qVerify(true, #statement, (description), __FILE__, __LINE__))\
            return false;\
    } else {\
        if (!QTest::qVerify(false, #statement, (description), __FILE__, __LINE__))\
            return false;\
    }\
} while (0)


class ScopedLoopbackManager
{
public:
    enum CreateMode
    {
        CreateFromLoopbackDevice,
        CreateFromImageFile
    };

    ScopedLoopbackManager()
    { }

    bool create(CreateMode mode, const QString &argument)
    {
        if (mode == CreateFromLoopbackDevice) {
            m_loopbackDevice = argument;
        } else if (mode == CreateFromImageFile) {
            SUB_QVERIFY2(!argument.isEmpty(), "");

            ScopedRootPrivileges sudo;

            QProcess p;
            p.start(LOSETUP, { "--show", "-f", argument });
            SUB_QVERIFY2(p.waitForStarted(processTimeout), qPrintable(p.errorString()));
            SUB_QVERIFY2(p.waitForFinished(processTimeout), qPrintable(p.errorString()));
            SUB_QVERIFY2(p.exitCode() == 0, qPrintable(p.readAllStandardError()));
            m_loopbackDevice = p.readAllStandardOutput().trimmed();
        }
        SUB_QVERIFY2(m_loopbackDevice.startsWith("/dev/loop"), qPrintable(m_loopbackDevice));
        return true;
    }

    void detach()
    {
        m_loopbackDevice.clear();
    }

    QString loopbackDevice()
    {
        return m_loopbackDevice;
    }

    ~ScopedLoopbackManager()
    {
        if (!m_loopbackDevice.isEmpty()) {
            ScopedRootPrivileges sudo;

            QProcess p;

            for (int tries = 2; tries; --tries) {
                p.start(LOSETUP, { "-d", m_loopbackDevice });
                QVERIFY2(p.waitForStarted(processTimeout), qPrintable(p.errorString()));
                QVERIFY2(p.waitForFinished(processTimeout), qPrintable(p.errorString()));
                if (p.exitCode() == 0)
                    return;

                QTest::qWait(200);
            }
            QFAIL(qPrintable(p.readAllStandardError()));
        }
    }


private:
    QString m_loopbackDevice;
};

class ScopedMountManager
{
public:
    enum CreateMode
    {
        CreateFromUnmounted,
        CreateFromMounted
    };

    ScopedMountManager()
    { }

    bool create(CreateMode mode, const QString &device, const QString &mountPoint)
    {
        m_device = device;
        m_mountPoint = mountPoint;

        SUB_QVERIFY2(QFile::exists(device), "");
        SUB_QVERIFY2(QDir(mountPoint).exists(), "");

        if (mode == CreateFromUnmounted) {
            ScopedRootPrivileges sudo;

            QProcess p;
            p.start(MOUNT, { "-t", "ext2", device, mountPoint});
            SUB_QVERIFY2(p.waitForStarted(processTimeout), qPrintable(p.errorString()));
            SUB_QVERIFY2(p.waitForFinished(processTimeout), qPrintable(p.errorString()));
            SUB_QVERIFY2(p.exitCode() == 0, qPrintable(p.readAllStandardError()));
        }
        return m_mounted = true;
    }

    void detach()
    {
        m_mounted = false;
    }

    ~ScopedMountManager()
    {
        if (m_mounted) {
            ScopedRootPrivileges sudo;

            QProcess p;
            for (int tries = 2; tries; --tries) {
                p.start(UMOUNT, { "-lf", m_mountPoint });
                QVERIFY2(p.waitForStarted(processTimeout), qPrintable(p.errorString()));
                QVERIFY2(p.waitForFinished(processTimeout), qPrintable(p.errorString()));
                if (p.exitCode() == 0)
                    return;

                QTest::qWait(200);
            }
            QFAIL(qPrintable(p.readAllStandardError()));
        }
    }

    bool checkKernelMountStatus()
    {
        return mountedDirectories().values(m_mountPoint).contains(m_device);
    }

private:
    QString m_device;
    QString m_mountPoint;
    bool m_mounted = false;
};

class tst_Sudo : public QObject
{
    Q_OBJECT

public:
    tst_Sudo(QObject *parent = nullptr);
    ~tst_Sudo();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void loopback_data();
    void loopback();
    void mount_data();
    void mount();
    void unmount_data();
    void unmount();
    void checkMountPoints(); // not really part of Sudo, but can be ideally tested here
    void image();

private:
    QString pathTo(const QString &file)
    {
        return QDir(m_workDir.path()).absoluteFilePath(file);
    }

    SudoClient *m_root = nullptr;
    TemporaryDir m_workDir;
};

tst_Sudo::tst_Sudo(QObject *parent)
    : QObject(parent)
{ }

tst_Sudo::~tst_Sudo()
{
    if (m_workDir.isValid()) {
        ScopedRootPrivileges sudo;

        detachLoopbacksAndUnmount(nullptr, m_workDir.path());
        recursiveOperation(m_workDir.path(), SafeRemove());
    }
}

void tst_Sudo::initTestCase()
{
    timeoutFactor = qMax(1, qEnvironmentVariableIntValue("TIMEOUT_FACTOR"));
    processTimeout *= timeoutFactor;
    qInfo() << "Timeouts are multiplied by" << timeoutFactor << "(changed by (un)setting $TIMEOUT_FACTOR)";

    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_root = SudoClient::instance();
    QVERIFY(m_root);
    if (SudoClient::instance()->isFallbackImplementation())
        QSKIP("Not running with root privileges - neither directly, or SUID-root, or sudo");

    QVERIFY(QFile::exists(LOSETUP));
    QVERIFY(QFile::exists(MOUNT));
    QVERIFY(QFile::exists(UMOUNT));
    QVERIFY(QFile::exists(BLKID));


    QVERIFY(m_workDir.isValid());

    for (int i = 0; i < 3; ++i) {
        QVERIFY(QDir(m_workDir.path()).mkdir(QString::fromLatin1("mp-%1").arg(i)));
        QVERIFY(QFile::copy(AM_TESTDATA_DIR "ext2image", QString::fromLatin1("%1/img-%2")
                            .arg(m_workDir.path()).arg(i)));
    }
}

void tst_Sudo::cleanupTestCase()
{
    // the real cleanup happens in ~tst_Installer, since we also need
    // to call this cleanup from the crash handler
}


void tst_Sudo::loopback_data()
{
    QTest::addColumn<QString>("imageFile");
    QTest::addColumn<bool>("readOnly");
    QTest::addColumn<bool>("valid");

    QTest::newRow("invalid-image") << pathTo("invalid") << false << false;
    QTest::newRow("read-write")    << pathTo("img-0") << false << true;
    QTest::newRow("read-only")     << pathTo("img-0") << true  << true;
}

void tst_Sudo::loopback()
{
    QFETCH(QString, imageFile);
    QFETCH(bool, readOnly);
    QFETCH(bool, valid);

    QString loopbackDevice = m_root->attachLoopback(imageFile, readOnly);
    QVERIFY2(valid == !loopbackDevice.isEmpty(), qPrintable(m_root->lastError()));
    if (!loopbackDevice.isEmpty()) {
        ScopedLoopbackManager slm;
        QVERIFY(slm.create(ScopedLoopbackManager::CreateFromLoopbackDevice, loopbackDevice));

        if (valid) {
            ScopedRootPrivileges sudo;

            struct loop_info64 loopInfo64;
            QFile f(loopbackDevice);
            QVERIFY(f.open(QFile::ReadOnly));
            QVERIFY2(::ioctl(f.handle(), LOOP_GET_STATUS64, &loopInfo64) == 0, strerror(errno));
            f.close();

            QTest::qWait(200);

            QVERIFY2(m_root->detachLoopback(loopbackDevice), qPrintable(m_root->lastError()));
            slm.detach();
            QVERIFY(!QFile::exists(slm.loopbackDevice()));
        }
    }
}


void tst_Sudo::mount_data()
{
    QTest::addColumn<QString>("imageFile");
    QTest::addColumn<QString>("mountPoint");
    QTest::addColumn<bool>("readOnly");

    QTest::newRow("missing-mountpoint") << pathTo("img-0") << "" << false;
    QTest::newRow("read-only")          << pathTo("img-0") << pathTo("mp-0") << true;
    QTest::newRow("read-write")         << pathTo("img-0") << pathTo("mp-0") << false;
}

void tst_Sudo::mount()
{
    QFETCH(QString, imageFile);
    QFETCH(QString, mountPoint);
    QFETCH(bool, readOnly);

    ScopedLoopbackManager slm;
    QVERIFY(slm.create(ScopedLoopbackManager::CreateFromImageFile, imageFile));
    QString loopbackDevice = slm.loopbackDevice();
    QVERIFY(imageFile.isEmpty() == loopbackDevice.isEmpty());

    bool mountOk = m_root->mount(loopbackDevice, mountPoint, readOnly);

    if (imageFile.isEmpty() || mountPoint.isEmpty()) {
        QVERIFY(!mountOk);
    } else {
        QVERIFY2(mountOk, qPrintable(m_root->lastError()));

        ScopedMountManager smm;
        QVERIFY(smm.create(ScopedMountManager::CreateFromMounted, loopbackDevice, mountPoint));
        QVERIFY(smm.checkKernelMountStatus());

        {
            ScopedRootPrivileges sudo;

            QFile file(mountPoint + "/write-test");
            bool canOpen = file.open(QIODevice::WriteOnly);
            QCOMPARE(readOnly, !canOpen);
            if (!readOnly) {
                QVERIFY(canOpen);
                QCOMPARE(file.write("foobar"), 6);
                file.close();
                QCOMPARE(file.size(), 6);
                QVERIFY(file.remove());
            }
        }
    }
}


void tst_Sudo::unmount_data()
{
    QTest::addColumn<bool>("openFile");
    QTest::addColumn<bool>("forceUnmount");
    QTest::addColumn<bool>("canUnmount");

    QTest::newRow("normal-no-force") << false << false << true;
    QTest::newRow("normal-force")    << false << true  << true;
    QTest::newRow("file-no-force")   << true  << false << false;
    QTest::newRow("file-force")      << true  << true  << true;
}

void tst_Sudo::unmount()
{
    QFETCH(bool, openFile);
    QFETCH(bool, forceUnmount);
    QFETCH(bool, canUnmount);

    ScopedLoopbackManager slm;
    QVERIFY(slm.create(ScopedLoopbackManager::CreateFromImageFile, pathTo("img-0")));
    QVERIFY(!slm.loopbackDevice().isEmpty());
    ScopedMountManager smm;
    QVERIFY(smm.create(ScopedMountManager::CreateFromUnmounted, slm.loopbackDevice(), pathTo("mp-0")));

    QFile file(pathTo("mp-0/force-lock"));
    if (openFile) {
        ScopedRootPrivileges sudo;

        QVERIFY(file.open(QFile::WriteOnly));
    }
    QTest::qWait(200);

    bool ok = m_root->unmount(pathTo("mp-0"), forceUnmount);
    if (ok)
        smm.detach();
    QVERIFY2(ok == canUnmount, qPrintable(m_root->lastError()));
    QCOMPARE(!smm.checkKernelMountStatus(), canUnmount);
}


void tst_Sudo::checkMountPoints()
{
    ScopedLoopbackManager slm[3];

    for (int i = 0; i < 3; ++i) {
        QVERIFY(slm[i].create(ScopedLoopbackManager::CreateFromImageFile, pathTo(QString::fromLatin1("img-%1").arg(i))));
        QVERIFY(!slm[i].loopbackDevice().isEmpty());
    }
    // we are mounting both img-1 and img-2 to mp-2 on purpose here!
    ScopedMountManager smm[3];

    QVERIFY(smm[0].create(ScopedMountManager::CreateFromUnmounted, slm[0].loopbackDevice(), pathTo("mp-1")));
    QVERIFY(smm[1].create(ScopedMountManager::CreateFromUnmounted, slm[1].loopbackDevice(), pathTo("mp-2")));
    QVERIFY(smm[2].create(ScopedMountManager::CreateFromUnmounted, slm[2].loopbackDevice(), pathTo("mp-2")));

    // map mount-points to loopback devices
    QList<int> mountPointList[3] = { { }, { 0 }, { 1, 2 } };

    for (int i = 0; i < 3; ++i)
        QVERIFY(smm[i].checkKernelMountStatus());

    QMultiMap<QString, QString> allMounts = mountedDirectories();

    for (int i = 0; i < 3; ++i) {
        QString mountPoint = pathTo(QString::fromLatin1("mp-%1").arg(i));
        QStringList mounts = allMounts.values(mountPoint);

        QCOMPARE(mounts.count(), mountPointList[i].size());
        foreach (int index, mountPointList[i])
            QVERIFY(mounts.contains(slm[index].loopbackDevice()));
    }
}


void tst_Sudo::image()
{
    QString imageFile = pathTo("img-new");

    QFile f(imageFile);
    f.remove();
    QVERIFY(!f.exists());
    QVERIFY2(f.open(QFile::WriteOnly | QFile::Truncate), qPrintable(f.errorString()));
    QVERIFY2(f.resize(1024*1024), qPrintable(f.errorString()));
    f.close();
    QCOMPARE(f.size(), qint64(1024*1024));

    {
        ScopedLoopbackManager slm;
        QVERIFY(slm.create(ScopedLoopbackManager::CreateFromImageFile, imageFile));

        QVERIFY2(m_root->mkfs(slm.loopbackDevice()), qPrintable(m_root->lastError()));

        ScopedRootPrivileges sudo;

        QProcess p;
        p.start(BLKID, { "-s", "TYPE", "-o", "value", slm.loopbackDevice() });
        QVERIFY2(p.waitForStarted(processTimeout), qPrintable(p.errorString()));
        QVERIFY2(p.waitForFinished(processTimeout), qPrintable(p.errorString()));
        QVERIFY(p.exitCode() == 0);
        QCOMPARE(p.readAllStandardOutput().trimmed(), QByteArray("ext2"));
    }
    QVERIFY(QFile::remove(imageFile));
}

static tst_Sudo *tstSudo = nullptr;

int main(int argc, char **argv)
{
    startedSudoServer = forkSudoServer(DropPrivilegesRegainable, &sudoServerError);

    QCoreApplication a(argc, argv);
    tstSudo = new tst_Sudo(&a);

#ifdef Q_OS_LINUX
    auto crashHandler = [](int sigNum) -> void {
        // we are doing very unsafe things from a within a signal handler, but
        // we've crashed anyway at this point and the alternative is that we are
        // leaking mounts and attached loopback devices.

        tstSudo->~tst_Sudo();

        if (sigNum != -1)
            exit(1);
    };

    signal(SIGABRT, crashHandler);
    signal(SIGSEGV, crashHandler);
    signal(SIGINT, crashHandler);
#endif // Q_OS_LINUX

    return QTest::qExec(tstSudo, argc, argv);
}

#include "tst_sudo.moc"
