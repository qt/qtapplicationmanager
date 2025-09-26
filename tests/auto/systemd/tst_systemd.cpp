// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QTest>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utilities.h"
#include "exception.h"
#include "systemd.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM


class tst_Systemd : public QObject
{
    Q_OBJECT

public:
    tst_Systemd(QObject *parent = nullptr);

private Q_SLOTS:
    void initTestCase();

    void noSystemd();
    void simulatedSystemd_data();
    void simulatedSystemd();

private:
    Systemd *m_sd = nullptr;
};


QT_BEGIN_NAMESPACE_AM
class SystemdTest
{
public:
    static void recreate(Systemd *sd)
    {
        sd->~Systemd();
        new (sd) Systemd;
    }
};
QT_END_NAMESPACE_AM


class DatagramSocket
{
public:
    DatagramSocket(const char *name) : m_name(name) { }
    ~DatagramSocket() { if (m_fd >= 0) ::close(m_fd); }
    QByteArray name() const { return m_name; }

    bool create()
    {
        if (m_fd >= 0)
            return false;

        if ((m_fd = ::socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0) {
            struct ::sockaddr_un sun;
            ::memset(&sun, 0, sizeof(sun));
            sun.sun_family = AF_UNIX;
            ::memcpy(sun.sun_path, m_name.constData(), m_name.size());
            if (m_name.startsWith('@'))
                sun.sun_path[0] = 0;
            if (::bind(m_fd, reinterpret_cast<sockaddr *>(&sun), sizeof(sun.sun_family) + m_name.size()) == 0)
                return true;
        }
        return false;
    }

    QByteArray read()
    {
        QByteArray buffer;
        buffer.resize(1024);
        int n = ::recvfrom(m_fd, buffer.data(), buffer.size(), 0, nullptr, nullptr);
        return buffer.left(n > 0 ? n : 0);
    }

private:
    QByteArray m_name;
    int m_fd = -1;
};


tst_Systemd::tst_Systemd(QObject *parent)
    : QObject(parent)
{ }

void tst_Systemd::initTestCase()
{
    static const std::array envVars = {
        "NOTIFY_SOCKET",
        "WATCHDOG_USEC",
        "WATCHDOG_PID",
        "LISTEN_FDS",
        "LISTEN_FDNAMES",
        "LISTEN_PID"
    };
    for (const char *envVar : envVars)
        QVERIFY2(!qEnvironmentVariableIsSet(envVar), envVar);

    m_sd = Systemd::instance();
    QVERIFY(m_sd);
}

void tst_Systemd::noSystemd()
{
    QVERIFY(!m_sd->notify(u"foo"_s));
    QVERIFY(!m_sd->watchdogTimeout(false));
    QVERIFY(!m_sd->watchdogTimeout(true));
    QVERIFY(m_sd->listenFds(QRegularExpression(u".*"_s), false).isEmpty());
    QVERIFY(m_sd->listenFds(QRegularExpression(u".*"_s), true).isEmpty());
}

void tst_Systemd::simulatedSystemd_data()
{
    QTest::addColumn<bool>("useCorrectPid");
    QTest::newRow("correct-pid") << true;
    QTest::newRow("wrong-pid") << false;
}

void tst_Systemd::simulatedSystemd()
{
    QFETCH(bool, useCorrectPid);

    // create an anonymous DGRAM socket for notify
    DatagramSocket notify("@qtam-systemd-test-socket");
    QVERIFY(notify.create());

    QByteArray pidstr = QByteArray::number(::getpid() + (useCorrectPid ? 0 : 1));
    qputenv("NOTIFY_SOCKET", notify.name());
    qputenv("WATCHDOG_USEC", "1000000");
    qputenv("WATCHDOG_PID", pidstr);
    qputenv("LISTEN_FDS", "2");
    qputenv("LISTEN_FDNAMES", "foo.bar:bar.foo");
    qputenv("LISTEN_PID", pidstr);

    // re-create the singleton
    SystemdTest::recreate(m_sd);

    QVERIFY(m_sd->notify(u"foo"_s));
    QCOMPARE(notify.read(), "foo");

    auto opt = m_sd->watchdogTimeout(false);
    if (useCorrectPid) {
        QVERIFY(opt);
        QCOMPARE(*opt, 1s);
    } else {
        QVERIFY(!opt);
    }
    opt = m_sd->watchdogTimeout(true);
    QVERIFY(opt);
    QCOMPARE(*opt, 1s);

    auto lfds = m_sd->listenFds(QRegularExpression(u".*"_s), false);
    QCOMPARE(lfds.size(), useCorrectPid ? 2 : 0);
    lfds = m_sd->listenFds(QRegularExpression(u".*"_s), true);
    QCOMPARE(lfds.size(), 2);
    lfds = m_sd->listenFds(QRegularExpression(u"^foo\\."_s), false);
    QCOMPARE(lfds.size(), useCorrectPid ? 1 : 0);
    lfds = m_sd->listenFds(QRegularExpression(u"^bar\\."_s), true);
    QCOMPARE(lfds.size(), 1);
    lfds = m_sd->listenFds(QRegularExpression(u"^nope\\."_s), true);
    QCOMPARE(lfds.size(), 0);
}

QTEST_APPLESS_MAIN(tst_Systemd)

#include "tst_systemd.moc"
