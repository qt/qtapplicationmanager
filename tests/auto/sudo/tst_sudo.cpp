// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtTest>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include "utilities.h"
#include "sudo.h"

QT_USE_NAMESPACE_AM

static int processTimeout = 3000;

static bool startedSudoServer = false;
static QString sudoServerError;

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


class tst_Sudo : public QObject
{
    Q_OBJECT

public:
    tst_Sudo(QObject *parent = nullptr);
    ~tst_Sudo();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void privileges();

private:
    SudoClient *m_sudo = nullptr;
};

tst_Sudo::tst_Sudo(QObject *parent)
    : QObject(parent)
{ }

tst_Sudo::~tst_Sudo()
{ }

void tst_Sudo::initTestCase()
{
    processTimeout *= timeoutFactor();

    QVERIFY2(startedSudoServer, qPrintable(sudoServerError));
    m_sudo = SudoClient::instance();
    QVERIFY(m_sudo);
    if (m_sudo->isFallbackImplementation())
        QSKIP("Not running with root privileges - neither directly, or SUID-root, or sudo");
}

void tst_Sudo::privileges()
{
    ScopedRootPrivileges sudo;
}

void tst_Sudo::cleanupTestCase()
{
    // the real cleanup happens in ~tst_Installer, since we also need
    // to call this cleanup from the crash handler
}

static tst_Sudo *tstSudo = nullptr;

int main(int argc, char **argv)
{
    try {
        Sudo::forkServer(Sudo::DropPrivilegesRegainable);
        startedSudoServer = true;
    } catch (...) { }

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
