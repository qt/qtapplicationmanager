// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <optional>

#ifndef SYS_open_tree
#  define SYS_open_tree 428
#endif
#ifndef OPEN_TREE_CLOEXEC
#  define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif
#ifndef SYS_pidfd_open
#  define SYS_pidfd_open 434
#endif

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include "utilities.h"
#include "unix-utilities.h"
#include "exception.h"
#include "sudo.h"

QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;


class tst_Sudo : public QObject
{
    Q_OBJECT

public:
    tst_Sudo(QObject *parent = nullptr);
    ~tst_Sudo() override;

    static bool startedSudoServer;
    static QString sudoServerError;
    // Targets we bind-mounted onto; swept in ~tst_Sudo (also reached from the crash handler) so a
    // crash mid-test cannot leak a kernel mount that would pin m_testRoot forever.
    static QStringList activeMounts;
    // Parked children holding a private mount namespace (bindMountIntoNamespace); killing the child
    // destroys its namespace and any mount inside it, so reaping them is the cleanup.
    static QList<pid_t> activeMountChildren;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void privileges();

    void trustedStateRoundtrip();
    void trustedStateOverwrite();
    void trustedStateRemove();
    void trustedStateRemoveMissing();
    void trustedStateSubdirectory();
    void trustedCacheRoundtrip();
    void trustedCacheRemove();
    void trustedCacheRemoveMissing();
    void trustedStateAndCacheAreSeparate();
    void trustedWriteDiscardedWithoutCommit();
    void trustedSaveCancelDiscards();
    void trustedSaveCommitAfterCancelThrows();
    void trustedCommitForeignFdRejected();
    void trustedDoubleCommitRejected();
    void trustedFilePathValidation_data();
    void trustedFilePathValidation();
    void trustedFileUnsupportedLocation();
    void trustedFileOwnership();
    void setInstanceIdSecondCallSameValueOk();
    void setInstanceIdSecondCallDifferentValueThrows();
    void removeRecursive();
    void removeRecursiveOutsideTestPrefixRejected();
    void setAllowedRemoveRootsSecondCallDifferentValueThrows();

    void bindMount_data();
    void bindMount();
    void bindMountMissing_data();
    void bindMountMissing();
    void bindMountPathValidation_data();
    void bindMountPathValidation();
    void bindMountIntoNamespace();

private:
    QString trustedStatePath(const QString &relPath) const;
    void writeTrustedState(const QString &relPath, const QByteArray &bytes);
    void writeTrustedCache(const QString &relPath, const QByteArray &bytes);
    QByteArray readTrustedState(const QString &relPath);
    QByteArray readTrustedCache(const QString &relPath);
    // Runs `body` briefly as root, then drops back. Returns whatever body() returned.
    template<typename F> auto sudo(F &&body);

    // bind-mount test helpers
    void skipIfNoBindMount();   // QSKIP (throws) in fallback mode or on a too-old kernel
    int unmount(const QString &target);   // best-effort umount as root
    // Checks <pid>'s mountinfo (default: our own); pid != self reads into another process's ns.
    bool isMountPoint(const QString &path, pid_t pid = 0);

    SudoClient *m_sudo = nullptr;
    QTemporaryDir m_testRoot;
    std::optional<bool> m_haveBindMount;   // cached kernel-support probe (open_tree/move_mount)
};

bool tst_Sudo::startedSudoServer = false;
QString tst_Sudo::sudoServerError;
QStringList tst_Sudo::activeMounts;
QList<pid_t> tst_Sudo::activeMountChildren;

tst_Sudo::tst_Sudo(QObject *parent)
    : QObject(parent)
{ }

tst_Sudo::~tst_Sudo()
{
    // The IPC helper writes root-owned files under <testRoot>/var/; only it can remove them
    // before QTemporaryDir's destructor sweeps the (now-empty) tree. In fallback mode the
    // files are user-owned under <testRoot>/<XDG dir>/ and QTemporaryDir handles them.
    if (m_sudo && !m_sudo->isFallbackImplementation()) {
        // Detach any bind mounts first - they are kernel objects QTemporaryDir cannot rmdir, and a
        // leaked mount pins m_testRoot. This must run before removeRecursive() sweeps the tree.
        for (const QString &target : std::as_const(activeMounts))
            unmount(target);
        activeMounts.clear();

        // Reap parked namespace children; their death tears down the namespace and its mounts.
        for (pid_t child : std::as_const(activeMountChildren)) {
            ::kill(child, SIGKILL);
            ::waitpid(child, nullptr, 0);
        }
        activeMountChildren.clear();

        try {
            m_sudo->removeRecursive(m_testRoot.path() + u"/var"_s);
        } catch (...) {
            // best effort; QTemporaryDir's dtor will try too
        }
    }
}

void tst_Sudo::initTestCase()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#endif

    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_sudo = SudoClient::instance();
    QVERIFY(m_sudo);
    QVERIFY(m_testRoot.isValid());

#if defined(QT_BUILD_INTERNAL)
    m_sudo->setTestRootPathPrefix(m_testRoot.path() + u'/');
#endif
    m_sudo->setInstanceId(QString());
}

void tst_Sudo::privileges()
{
    if (m_sudo->isFallbackImplementation())
        QSKIP("not running with root privileges");

    uid_t uid = getuid();
    gid_t gid = getgid();
    if (setresuid(0, 0, 0) || setresgid(0, 0, 0))
        QFAIL("cannot re-gain root privileges");

    if (setresgid(gid, gid, 0) || setresuid(uid, uid, 0))
        QFAIL("cannot drop root privileges");
}

QString tst_Sudo::trustedStatePath(const QString &relPath) const
{
    return m_testRoot.path() + u"/var/lib/qtapplicationmanager/"_s + relPath;
}

void tst_Sudo::writeTrustedState(const QString &relPath, const QByteArray &bytes)
{
    auto w = m_sudo->openTrustedSaveFile(QStandardPaths::StateLocation, relPath);
    QVERIFY(w);
    QCOMPARE(w->write(bytes), qint64(bytes.size()));
    w->commit();
}

void tst_Sudo::writeTrustedCache(const QString &relPath, const QByteArray &bytes)
{
    auto w = m_sudo->openTrustedSaveFile(QStandardPaths::CacheLocation, relPath);
    QVERIFY(w);
    QCOMPARE(w->write(bytes), qint64(bytes.size()));
    w->commit();
}

QByteArray tst_Sudo::readTrustedState(const QString &relPath)
{
    auto r = m_sudo->openTrustedFile(QStandardPaths::StateLocation, relPath);
    return r ? r->readAll() : QByteArray();
}

QByteArray tst_Sudo::readTrustedCache(const QString &relPath)
{
    auto r = m_sudo->openTrustedFile(QStandardPaths::CacheLocation, relPath);
    return r ? r->readAll() : QByteArray();
}

template<typename F>
auto tst_Sudo::sudo(F &&body)
{
    uid_t uid = ::getuid();
    gid_t gid = ::getgid();
    if (::setresuid(0, 0, 0) || ::setresgid(0, 0, 0))
        QTest::qFail("cannot re-gain root privileges", __FILE__, __LINE__);
    auto guard = qScopeGuard([uid, gid] {
        // drop back to the test user - a stuck root context would corrupt later tests
        if (::setresgid(gid, gid, 0) || ::setresuid(uid, uid, 0))
            QTest::qFail("cannot drop root privileges", __FILE__, __LINE__);
    });
    return body();
}

void tst_Sudo::trustedStateRoundtrip()
{
    const QByteArray payload = "hello trusted world\n";
    const QString rel = u"installation-reports/roundtrip.yaml"_s;

    writeTrustedState(rel, payload);
    QCOMPARE(readTrustedState(rel), payload);

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
}

void tst_Sudo::trustedStateOverwrite()
{
    const QString rel = u"installation-reports/overwrite.yaml"_s;

    writeTrustedState(rel, "first");
    writeTrustedState(rel, "second");
    QCOMPARE(readTrustedState(rel), QByteArray("second"));

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
}

void tst_Sudo::trustedStateRemove()
{
    const QString rel = u"installation-reports/to-remove.yaml"_s;

    writeTrustedState(rel, "x");
    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);

    bool threw = false;
    try {
        readTrustedState(rel);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "reading a removed trusted file should throw");
}

void tst_Sudo::trustedStateRemoveMissing()
{
    // Removing a file that doesn't exist must not throw.
    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, u"installation-reports/never-existed.yaml"_s);
}

void tst_Sudo::trustedStateSubdirectory()
{
    const QString rel = u"sub/dir/file.bin"_s;
    const QByteArray payload = QByteArray::fromHex("deadbeefcafe");

    writeTrustedState(rel, payload);
    QCOMPARE(readTrustedState(rel), payload);

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
}

void tst_Sudo::trustedCacheRoundtrip()
{
    const QByteArray payload = "cached bytes";
    const QString rel = u"roundtrip.cache"_s;

    writeTrustedCache(rel, payload);
    QCOMPARE(readTrustedCache(rel), payload);

    m_sudo->removeTrustedFile(QStandardPaths::CacheLocation, rel);
}

void tst_Sudo::trustedCacheRemove()
{
    const QString rel = u"to-remove.cache"_s;

    writeTrustedCache(rel, "x");
    m_sudo->removeTrustedFile(QStandardPaths::CacheLocation, rel);

    bool threw = false;
    try {
        readTrustedCache(rel);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "reading a removed trusted cache file should throw");
}

void tst_Sudo::trustedCacheRemoveMissing()
{
    // Removing a file that doesn't exist must not throw.
    m_sudo->removeTrustedFile(QStandardPaths::CacheLocation, u"never-existed.cache"_s);
}

void tst_Sudo::trustedStateAndCacheAreSeparate()
{
    const QString rel = u"namespaced.bin"_s;

    writeTrustedState(rel, "state-value");
    writeTrustedCache(rel, "cache-value");

    QCOMPARE(readTrustedState(rel), QByteArray("state-value"));
    QCOMPARE(readTrustedCache(rel), QByteArray("cache-value"));

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
    m_sudo->removeTrustedFile(QStandardPaths::CacheLocation, rel);
}

void tst_Sudo::trustedWriteDiscardedWithoutCommit()
{
    const QString rel = u"installation-reports/uncommitted.yaml"_s;

    // Drop the write handle without commit(): the content must be discarded (nothing materializes).
    {
        auto w = m_sudo->openTrustedSaveFile(QStandardPaths::StateLocation, rel);
        QVERIFY(w);
        QCOMPARE(w->write("partial"), qint64(7));
        // no commit() - w goes out of scope here
    }

    bool threw = false;
    try {
        readTrustedState(rel);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "an uncommitted trusted write must not materialize a file");
}

void tst_Sudo::trustedSaveCancelDiscards()
{
    const QString rel = u"installation-reports/cancelled.yaml"_s;
    {
        auto w = m_sudo->openTrustedSaveFile(QStandardPaths::StateLocation, rel);
        QVERIFY(w);
        QCOMPARE(w->write("partial"), qint64(7));
        w->cancel(); // explicit discard - must not materialize, must not warn on destruction
    }

    bool threw = false;
    try {
        readTrustedState(rel);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "a cancelled trusted write must not materialize a file");
}

void tst_Sudo::trustedSaveCommitAfterCancelThrows()
{
    const QString rel = u"installation-reports/cancel-then-commit.yaml"_s;
    auto w = m_sudo->openTrustedSaveFile(QStandardPaths::StateLocation, rel);
    QVERIFY(w);
    QCOMPARE(w->write("x"), qint64(1));
    w->cancel();

    bool threw = false;
    try {
        w->commit();
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "commit() after cancel() must throw");

    bool readThrew = false;
    try {
        readTrustedState(rel);
    } catch (const Exception &) {
        readThrew = true;
    }
    QVERIFY2(readThrew, "a cancelled trusted write must not materialize a file");
}

void tst_Sudo::trustedCommitForeignFdRejected()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#else
    if (m_sudo->isFallbackImplementation())
        QSKIP("staging-session validation only applies to the root helper");

    // /etc/passwd is a root-owned regular file the AM user can open read-only: it would pass a naive
    // {regular, uid 0} check, but it was never issued by openTrustedSaveFile, so commit must reject it.
    QFile passwd(u"/etc/passwd"_s);
    QVERIFY(passwd.open(QIODevice::ReadOnly));

    bool threw = false;
    try {
        m_sudo->commitRawFdForTest(passwd.handle());
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "committing a fd the helper never issued must be rejected");
#endif // QT_BUILD_INTERNAL
}

void tst_Sudo::trustedDoubleCommitRejected()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#else
    if (m_sudo->isFallbackImplementation())
        QSKIP("staging-session one-shot only applies to the root helper");

    const QString rel = u"installation-reports/double-commit.yaml"_s;
    auto w = m_sudo->openTrustedSaveFile(QStandardPaths::StateLocation, rel);
    QVERIFY(w);
    QCOMPARE(w->write("once"), qint64(4));
    const int fd = w->handle();
    w->commit(); // first commit consumes the staging session

    bool threw = false;
    try {
        m_sudo->commitRawFdForTest(fd); // same fd again -> session already gone
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "a second commit of the same staging fd must be rejected (one-shot)");

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
#endif // QT_BUILD_INTERNAL
}

void tst_Sudo::trustedFilePathValidation_data()
{
    QTest::addColumn<QString>("relPath");
    QTest::newRow("empty") << QString();
    QTest::newRow("absolute") << u"/etc/passwd"_s;
    QTest::newRow("dotdot at root") << u"../etc/passwd"_s;
    QTest::newRow("dotdot mid path") << u"installation-reports/../../../etc/passwd"_s;
}

void tst_Sudo::trustedFilePathValidation()
{
    QFETCH(QString, relPath);

    bool threwState = false;
    try {
        m_sudo->openTrustedFile(QStandardPaths::StateLocation, relPath);
    } catch (const Exception &) {
        threwState = true;
    }
    QVERIFY2(threwState, qPrintable(u"openTrustedFile(StateLocation) must reject: "_s + relPath));

    bool threwCache = false;
    try {
        m_sudo->openTrustedSaveFile(QStandardPaths::CacheLocation, relPath);
    } catch (const Exception &) {
        threwCache = true;
    }
    QVERIFY2(threwCache, qPrintable(u"openTrustedSaveFile(CacheLocation) must reject: "_s + relPath));
}

void tst_Sudo::trustedFileUnsupportedLocation()
{
    // Only StateLocation and CacheLocation are mapped to trusted trees; anything else must be
    // rejected (consistently in both helper and fallback mode).
    bool threw = false;
    try {
        m_sudo->openTrustedFile(QStandardPaths::HomeLocation, u"x"_s);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY2(threw, "an unsupported QStandardPaths location must be rejected");
}

void tst_Sudo::trustedFileOwnership()
{
    if (m_sudo->isFallbackImplementation())
        QSKIP("file ownership/perms guarantee only applies when running with root privileges");

    const QString rel = u"ownership-check.yaml"_s;

    writeTrustedState(rel, "x");

    const QString fullPath = trustedStatePath(rel);
    const QString parentDir = QFileInfo(fullPath).absolutePath();

    // The trusted tree is root:root 0700 / 0400 — we need root to stat through it.
    sudo([&] {
        struct ::stat dirSt { };
        QVERIFY(::stat(parentDir.toLocal8Bit().constData(), &dirSt) == 0);
        QCOMPARE(dirSt.st_uid, uid_t(0));
        QCOMPARE(dirSt.st_gid, gid_t(0));
        QCOMPARE(dirSt.st_mode & 0777, mode_t(0700));

        struct ::stat fileSt { };
        QVERIFY(::stat(fullPath.toLocal8Bit().constData(), &fileSt) == 0);
        QCOMPARE(fileSt.st_uid, uid_t(0));
        QCOMPARE(fileSt.st_gid, gid_t(0));
        QCOMPARE(fileSt.st_mode & 0777, mode_t(0400));
    });

    m_sudo->removeTrustedFile(QStandardPaths::StateLocation, rel);
}

// setInstanceId is a one-shot latch: calling it again with the same value is a no-op, but a
// different value throws. The singleton SudoClient was already pinned to QString() in initTestCase
// and there is no way to roll that back, so we exercise the latch against that fixed value. The
// "must be called before any trusted-file op" guard is covered implicitly: initTestCase sets it
// before any op runs, and trustedFilePathValidation/trustedPath reject everything else.

void tst_Sudo::setInstanceIdSecondCallSameValueOk()
{
    m_sudo->setInstanceId(QString()); // same value as initTestCase -> no-op, must not throw
}

void tst_Sudo::setInstanceIdSecondCallDifferentValueThrows()
{
    bool threw = false;
    try {
        m_sudo->setInstanceId(u"a-different-instance"_s);
    } catch (const Exception &) {
        threw = true;
    }
    QVERIFY(threw);
}

// removeRecursive deletes a whole subtree (nested dirs + files) and, for a symlink inside the tree,
// removes the link itself without following it - so the link's external target must survive. The
// tree lives under m_testRoot, so it is allowed via the developer-build testPrefix branch.
void tst_Sudo::removeRecursive()
{
    const QString tree = m_testRoot.path() + u"/tree"_s;
    QDir treeDir(tree);
    QVERIFY(treeDir.mkpath(tree + u"/sub/deeper"_s));
    QVERIFY(QFile(tree + u"/top.txt"_s).open(QIODevice::WriteOnly));
    QVERIFY(QFile(tree + u"/sub/deeper/leaf.txt"_s).open(QIODevice::WriteOnly));

#if defined(Q_OS_UNIX)
    // a file outside the tree, reached only via a symlink planted inside it (Unix only)
    QTemporaryFile external;
    QVERIFY(external.open());
    external.write("survive");
    external.close();
    QVERIFY(QFile::link(external.fileName(), tree + u"/sub/escape"_s));

    // a directory *with contents* outside the tree, reached only via a symlink planted inside it.
    // removeRecursive must unlink the symlink, not follow it and delete the target's files - so
    // exercise the symlink-to-dir guard that the file symlink above never reaches (isDir() is false
    // for a file symlink regardless of the guard).
    QTemporaryDir externalDir;
    QVERIFY(externalDir.isValid());
    const QString externalDirKeep = externalDir.path() + u"/keep.txt"_s;
    QVERIFY(QFile(externalDirKeep).open(QIODevice::WriteOnly));
    QVERIFY(QFile::link(externalDir.path(), tree + u"/sub/escape-dir"_s));
#endif

    QVERIFY(QFileInfo::exists(tree));
    m_sudo->removeRecursive(tree);

    QVERIFY(!QFileInfo::exists(tree));
#if defined(Q_OS_UNIX)
    QVERIFY2(QFileInfo::exists(external.fileName()), "removeRecursive must not follow symlinks out of the tree");
    QVERIFY2(QFileInfo::exists(externalDirKeep),
             "removeRecursive must not follow a directory symlink out of the tree");
#endif
}

// removeRecursive is confined to the allowed roots (none set here) or, in developer builds,
// anything under the testPrefix. A target outside both must be rejected before anything is deleted.
// We point at a sibling temp dir that exists but lives outside m_testRoot, and verify it survives.
void tst_Sudo::removeRecursiveOutsideTestPrefixRejected()
{
    QTemporaryDir outside;
    QVERIFY(outside.isValid());
    QVERIFY(!QString(outside.path() + u'/').startsWith(m_testRoot.path() + u'/'));

    QVERIFY_THROWS_EXCEPTION(Exception, m_sudo->removeRecursive(outside.path()));
    QVERIFY(QFileInfo::exists(outside.path()));
}

// setAllowedRemoveRecursiveRoots can only be set once, like setInstanceId: a second call with a
// different value throws. This test never sets roots elsewhere, so the first call here pins the
// value; the differing second call must throw.
void tst_Sudo::setAllowedRemoveRootsSecondCallDifferentValueThrows()
{
    m_sudo->setAllowedRemoveRecursiveRoots({ m_testRoot.path() });
    m_sudo->setAllowedRemoveRecursiveRoots({ m_testRoot.path() }); // same value -> no-op

    QVERIFY_THROWS_EXCEPTION(Exception,
                             m_sudo->setAllowedRemoveRecursiveRoots({ m_testRoot.path() + u"/other"_s }));
}

void tst_Sudo::skipIfNoBindMount()
{
    if (m_sudo->isFallbackImplementation())
        QSKIP("bindMountFileSystem requires root privileges");

    // Probe open_tree once: old CI kernels lack it (and move_mount), so skip rather than fail.
    // Only ENOSYS means "no such syscall" - any other error means the syscall exists (the probe
    // may legitimately fail for other reasons, e.g. we run as the dropped user here).
    if (!m_haveBindMount) {
        Unix::Fd fd = { int(::syscall(SYS_open_tree, AT_FDCWD, qPrintable(m_testRoot.path()),
                                      OPEN_TREE_CLOEXEC)) };
        m_haveBindMount = fd || (errno != ENOSYS);
    }
    if (!*m_haveBindMount)
        QSKIP("kernel lacks open_tree/move_mount");
}

int tst_Sudo::unmount(const QString &target)
{
    // umount2 needs root and the path may already be gone (crash sweep, double call) - ignore errors.
    return sudo([&] { return ::umount2(qPrintable(target), MNT_DETACH); });
}

bool tst_Sudo::isMountPoint(const QString &path, pid_t pid)
{
    // Scan the process's mountinfo for the mount point in field 5 (the canonical path is what
    // move_mount records). Passing another process's pid reads that process's mount namespace.
    const QByteArray needle = QDir::cleanPath(path).toLocal8Bit();
    const QString miPath = pid ? u"/proc/%1/mountinfo"_s.arg(pid) : u"/proc/self/mountinfo"_s;
    QFile mi(miPath);
    if (!mi.open(QIODevice::ReadOnly))
        return false;
    const auto lines = mi.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const auto fields = line.split(' ');
        if ((fields.size() > 4) && (fields.at(4) == needle))
            return true;
    }
    return false;
}

void tst_Sudo::bindMount_data()
{
    QTest::addColumn<bool>("readOnly");
    QTest::newRow("read-write") << false;
    QTest::newRow("read-only") << true;
}

// Bind-mount <src> onto <dst>, verify the source content shows through, and that detaching removes
// the mount again. A read-write mount must accept writes (landing in the source); a read-only mount
// must reject them with EROFS. Root-only: bindMountFileSystem throws in fallback mode.
void tst_Sudo::bindMount()
{
    skipIfNoBindMount();

    QFETCH(bool, readOnly);

    const QString base = m_testRoot.path() + u"/mnt-"_s + QString::fromLatin1(QTest::currentDataTag());
    const QString src = base + u"/src"_s;
    const QString dst = base + u"/dst"_s;
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));

    QFile marker(src + u"/data.txt"_s);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write(QByteArray("payload")), 7);
    marker.close();

    // Before the mount, dst is empty.
    QVERIFY(!QFileInfo::exists(dst + u"/data.txt"_s));

    m_sudo->bindMountFileSystem(src, dst, readOnly, /*namespacePidFd*/ -1);
    activeMounts << dst;
    auto guard = qScopeGuard([&] { unmount(dst); activeMounts.removeAll(dst); });

    QVERIFY(isMountPoint(dst));

    // The source content shows through in either mode.
    QFile seen(dst + u"/data.txt"_s);
    QVERIFY(seen.open(QIODevice::ReadOnly));
    QCOMPARE(seen.readAll(), QByteArray("payload"));
    seen.close();

    Unix::Fd fd { int(::open((dst + u"/data.txt"_s).toLocal8Bit().constData(), O_WRONLY)) };
    if (readOnly) {
        // Writing into the read-only mount must fail with EROFS (checked while still mounted).
        QVERIFY(!fd);
        QCOMPARE(errno, EROFS);
    } else {
        // A read-write mount must accept writes, and they must land in the source (real bind mount).
        QVERIFY(fd);
        QCOMPARE(::write(fd.get(), "changed", 7), 7);
        fd.reset();

        QFile fromSource(src + u"/data.txt"_s);
        QVERIFY(fromSource.open(QIODevice::ReadOnly));
        QCOMPARE(fromSource.readAll(), QByteArray("changed"));
        fromSource.close();
    }

    // Detaching must remove the mount and hide the source content again - in both modes.
    guard.dismiss();
    unmount(dst);
    activeMounts.removeAll(dst);
    QVERIFY(!isMountPoint(dst));
    QVERIFY(!QFileInfo::exists(dst + u"/data.txt"_s));
}

void tst_Sudo::bindMountMissing_data()
{
    QTest::addColumn<bool>("missingSource");
    QTest::newRow("source") << true;
    QTest::newRow("target") << false;
}

// A non-existent source or target cannot be opened, so the mount must fail before anything happens.
// The two cases fail at different points: the source in the parent's openPath(), the target later
// in the no-namespace branch's openPath().
void tst_Sudo::bindMountMissing()
{
    skipIfNoBindMount();

    QFETCH(bool, missingSource);

    const QString base = m_testRoot.path() + u"/mnt-missing-"_s + QString::fromLatin1(QTest::currentDataTag());
    const QString src = base + u"/src"_s;
    const QString dst = base + u"/dst"_s;
    // Create only the endpoint that is supposed to exist.
    QVERIFY(QDir().mkpath(missingSource ? dst : src));

    QVERIFY_THROWS_EXCEPTION(Exception,
                             m_sudo->bindMountFileSystem(src, dst, false, -1));
    // Confirm nothing was mounted onto the target (a missing target is trivially not a mountpoint).
    QVERIFY(!isMountPoint(dst));
}

void tst_Sudo::bindMountPathValidation_data()
{
    QTest::addColumn<bool>("badSource"); // false -> the target is the bad one
    QTest::addColumn<QString>("relativePath"); // non-empty -> relative-path case
    QTest::addColumn<bool>("parentSymlink"); // true -> plant a parent symlink

    QTest::newRow("relative source") << true << u"./relative/src"_s << false;
    QTest::newRow("relative target") << false << u"./relative/dst"_s << false;
    QTest::newRow("parent-symlink source") << true << QString() << true;
    QTest::newRow("parent-symlink target") << false << QString() << true;
}

// The hardening: a non-absolute path, or a path that needs no cleanup yet whose *parent* is a
// symlink (the real CVE - cleanPath leaves it untouched, O_NOFOLLOW opens the final component, but
// the /proc/self/fd readlink reveals the redirect), must be rejected before any mount is created.
void tst_Sudo::bindMountPathValidation()
{
    skipIfNoBindMount();

    QFETCH(bool, badSource);
    QFETCH(QString, relativePath);
    QFETCH(bool, parentSymlink);

    // Unique base per data row: the parent-symlink rows create an "evil" symlink that would
    // otherwise collide across rows.
    const QString base = m_testRoot.path() + u"/mnt-validate-"_s + QString::fromLatin1(QTest::currentDataTag());
    const QString realSrc = base + u"/src"_s;
    const QString realDst = base + u"/dst"_s;
    QVERIFY(QDir().mkpath(realSrc));
    QVERIFY(QDir().mkpath(realDst));
    QVERIFY(QFile(realSrc + u"/data.txt"_s).open(QIODevice::WriteOnly));

    QString src = realSrc;
    QString dst = realDst;

    if (!relativePath.isEmpty()) {
        (badSource ? src : dst) = relativePath; // a relative path must be rejected outright
    } else if (parentSymlink) {
        // <base>/evil -> <base>/real_parent ; the bad path is <base>/evil/leaf, which cleanPath
        // leaves unchanged (no . or .. to collapse) but resolves through the symlink to
        // <base>/real_parent/leaf.
        const QString realParent = base + u"/real_parent"_s;
        QVERIFY(QDir().mkpath(realParent + u"/leaf"_s));
        QVERIFY(QFile(realParent + u"/leaf/data.txt"_s).open(QIODevice::WriteOnly));
        QVERIFY(QFile::link(realParent, base + u"/evil"_s));
        (badSource ? src : dst) = base + u"/evil/leaf"_s;
    }

    activeMounts << dst;
    QVERIFY_THROWS_EXCEPTION(Exception, m_sudo->bindMountFileSystem(src, dst, false, -1));
    activeMounts.removeAll(dst);

    // Nothing must have been mounted onto the real target on the way to rejecting.
    QVERIFY(!isMountPoint(realDst));
}

// The namespace branch: fork a child that unshares its own mount namespace and parks, hand its
// pidfd to bindMountFileSystem, and verify the mount lands inside *that* child's namespace - not
// ours. This exercises the helper's fork()+setns()+validate+move_mount path that the no-namespace
// tests cannot reach.
void tst_Sudo::bindMountIntoNamespace()
{
    skipIfNoBindMount();

    const QString base = m_testRoot.path() + u"/mnt-ns"_s;
    const QString src = base + u"/src"_s;
    const QString dst = base + u"/dst"_s;
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    QFile marker(src + u"/data.txt"_s);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write(QByteArray("ns-payload"));
    marker.close();

    // ready[0]/[0] tells us the child has its namespace set up; park[0] keeps the child alive
    // until we close park[1] (so it never outlives this test even if we forget to kill it).
    int readyPipe[2] = { -1, -1 };
    int parkPipe[2] = { -1, -1 };
    QVERIFY(::pipe(readyPipe) == 0);
    QVERIFY(::pipe(parkPipe) == 0);

    pid_t child = ::fork();
    QVERIFY(child >= 0);
    if (child == 0) {
        // child: get a private mount namespace, then park
        ::close(readyPipe[0]);
        ::close(parkPipe[1]);

        // unshare()/mount() need CAP_SYS_ADMIN, but the main process dropped to the test user
        // (DropPrivilegesRegainable keeps saved-set-uid 0), so regain root first.
        if (::setresuid(0, 0, 0) != 0)
            ::_exit(2);

        // Without making the tree MS_SLAVE, a mount into our namespace can propagate back to the
        // host (systemd makes / shared). MS_SLAVE = receive from host, never send back.
        if ((::unshare(CLONE_NEWNS) != 0)
            || (::mount(nullptr, "/", nullptr, MS_REC | MS_SLAVE, nullptr) != 0)) {
            ::_exit(1);
        }
        char ok = 1;
        if (::write(readyPipe[1], &ok, 1) != 1)
            ::_exit(3);

        // block until the parent closes parkPipe[1] (test done) - read returns 0 on EOF
        char discard;
        while (::read(parkPipe[0], &discard, 1) > 0) { }
        ::_exit(0);
    }

    // parent
    ::close(readyPipe[1]);
    ::close(parkPipe[0]);
    activeMountChildren << child;
    auto childGuard = qScopeGuard([&] {
        ::close(parkPipe[1]); // release the child's park-read so it exits cleanly
        ::kill(child, SIGKILL);
        ::waitpid(child, nullptr, 0);
        activeMountChildren.removeAll(child);
    });

    char ready = 0;
    const ssize_t readyRead = ::read(readyPipe[0], &ready, 1); // child set up its namespace
    ::close(readyPipe[0]); // done with the ready pipe
    QCOMPARE(readyRead, 1);
    QCOMPARE(ready, char(1));

    int pidFd = int(::syscall(SYS_pidfd_open, child, 0));
    QVERIFY(pidFd >= 0);
    auto pidFdGuard = qScopeGuard([&] { ::close(pidFd); });

    m_sudo->bindMountFileSystem(src, dst, /*readOnly*/ false, pidFd);

    // The mount must be visible in the child's namespace ...
    QVERIFY(isMountPoint(dst, child));
    // ... and must NOT have leaked into our own namespace.
    QVERIFY(!isMountPoint(dst));
}

void tst_Sudo::cleanupTestCase()
{
    // the real cleanup happens in ~tst_Sudo, since we also need
    // to call this cleanup from the crash handler
}

static tst_Sudo *tstSudo = nullptr;

int main(int argc, char **argv)
{
    try {
        Sudo::forkServer(Sudo::DropPrivilegesRegainable);
        tst_Sudo::startedSudoServer = true;
    } catch (const Exception &e) {
        tst_Sudo::sudoServerError = e.errorString();
    }

    QCoreApplication a(argc, argv);

    try {
        Sudo::startServer();
    } catch (const Exception &e) {
        tst_Sudo::startedSudoServer = false;
        tst_Sudo::sudoServerError = e.errorString();
    }

    tstSudo = new tst_Sudo(&a);

#ifdef Q_OS_LINUX
    auto crashHandler = [](int sigNum) -> void {
        // we are doing very unsafe things from a within a signal handler, but
        // we've crashed anyway at this point and the alternative is that we are
        // potentially leaking mounts.

        if (tstSudo)
            tstSudo->~tst_Sudo();

        if (sigNum != -1)
            exit(1);
    };

    signal(SIGABRT, crashHandler);
    signal(SIGSEGV, crashHandler);
    signal(SIGINT, crashHandler);
#endif // Q_OS_LINUX

    int result = QTest::qExec(tstSudo, argc, argv);

    // Tear down the test object while QCoreApplication is still alive: ~tst_Sudo calls IPC,
    // and the global QThreadPool that QFuture::waitForFinished requires is deleted inside
    // ~QCoreApplication before children get cleaned up.
    delete tstSudo;
    tstSudo = nullptr;

    return result;
}

#include "tst_sudo.moc"
