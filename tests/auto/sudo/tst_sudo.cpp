// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include "utilities.h"
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
    void removeRecursiveOutsideTestPrefixRejected();
    void setAllowedRemoveRootsSecondCallDifferentValueThrows();

private:
    QString trustedStatePath(const QString &relPath) const;
    void writeTrustedState(const QString &relPath, const QByteArray &bytes);
    void writeTrustedCache(const QString &relPath, const QByteArray &bytes);
    QByteArray readTrustedState(const QString &relPath);
    QByteArray readTrustedCache(const QString &relPath);
    // Runs `body` briefly as root, then drops back. Returns whatever body() returned.
    template<typename F> auto sudo(F &&body);

    SudoClient *m_sudo = nullptr;
    QTemporaryDir m_testRoot;
};

bool tst_Sudo::startedSudoServer = false;
QString tst_Sudo::sudoServerError;

tst_Sudo::tst_Sudo(QObject *parent)
    : QObject(parent)
{ }

tst_Sudo::~tst_Sudo()
{
    // The IPC helper writes root-owned files under <testRoot>/var/; only it can remove them
    // before QTemporaryDir's destructor sweeps the (now-empty) tree. In fallback mode the
    // files are user-owned under <testRoot>/<XDG dir>/ and QTemporaryDir handles them.
    if (m_sudo && !m_sudo->isFallbackImplementation()) {
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
