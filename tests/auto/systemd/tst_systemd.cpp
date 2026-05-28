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
    void parseEnvironmentFile_data();
    void parseEnvironmentFile();

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

using StringMap = QMap<QString, QString>;
Q_DECLARE_METATYPE(StringMap)

void tst_Systemd::parseEnvironmentFile_data()
{
    QTest::addColumn<QString>("contents");
    QTest::addColumn<StringMap>("expected");

    auto map = [](std::initializer_list<std::pair<QString, QString>> items) {
        StringMap m;
        for (const auto &p : items)
            m.insert(p.first, p.second);
        return m;
    };

    QTest::newRow("empty")
        << QString()
        << StringMap { };

    QTest::newRow("empty-key")
        << u"=bar\n"_s
        << StringMap{};

    QTest::newRow("simple")
        << u"FOO=bar\n"_s
        << map({{ u"FOO"_s, u"bar"_s }});

    QTest::newRow("no-trailing-newline")
        << u"FOO=bar"_s
        << map({{ u"FOO"_s, u"bar"_s }});

    QTest::newRow("multiple-lines")
        << u"A=1\nB=2\nC=3\n"_s
        << map({{ u"A"_s, u"1"_s }, { u"B"_s, u"2"_s }, { u"C"_s, u"3"_s }});

    QTest::newRow("blank-and-comment-lines")
        << u"# header\n\nA=1\n; semi-comment\nB=2\n"_s
        << map({{ u"A"_s, u"1"_s }, { u"B"_s, u"2"_s }});

    QTest::newRow("hash-after-equals-is-not-comment")
        << u"URL=http://example.com/#frag\n"_s
        << map({{ u"URL"_s, u"http://example.com/#frag"_s }});

    QTest::newRow("value-with-equals")
        << u"TOKEN=eyJhbGciOiJIUzI1NiJ9.eyJpc3M=\n"_s
        << map({{ u"TOKEN"_s, u"eyJhbGciOiJIUzI1NiJ9.eyJpc3M="_s }});

    QTest::newRow("trailing-whitespace-on-key")
        << u"KEY   =val\n"_s
        << map({{ u"KEY"_s, u"val"_s }});

    QTest::newRow("leading-whitespace-on-value")
        << u"K=   val\n"_s
        << map({{ u"K"_s, u"val"_s }});

    QTest::newRow("trailing-whitespace-on-unquoted-value")
        << u"K=val   \n"_s
        << map({{ u"K"_s, u"val"_s }});

    QTest::newRow("interior-whitespace-preserved")
        << u"K=a  b  c\n"_s
        << map({{ u"K"_s, u"a  b  c"_s }});

    QTest::newRow("empty-value")
        << u"K=\n"_s
        << map({{ u"K"_s, QString() }});

    QTest::newRow("empty-double-quoted")
        << u"K=\"\"\n"_s
        << map({{ u"K"_s, QString() }});

    QTest::newRow("empty-single-quoted")
        << u"K=''\n"_s
        << map({{ u"K"_s, QString() }});

    QTest::newRow("double-quoted")
        << u"K=\"hello world\"\n"_s
        << map({{ u"K"_s, u"hello world"_s }});

    QTest::newRow("single-quoted-verbatim")
        << u"K='hello \"$dollar\" `tick` \\backslash'\n"_s
        << map({{ u"K"_s, u"hello \"$dollar\" `tick` \\backslash"_s }});

    QTest::newRow("double-quoted-shell-escapes")
        << u"K=\"a \\\"q\\\" b \\\\ \\$ \\`\"\n"_s
        << map({{ u"K"_s, u"a \"q\" b \\ $ `"_s }});

    QTest::newRow("double-quoted-preserves-unknown-backslash")
        << u"K=\"a\\nb\"\n"_s
        // '\n' (escape + n) inside double quotes is not in SHELL_NEED_ESCAPE,
        // so systemd preserves BOTH the backslash and the 'n' verbatim.
        << map({{ u"K"_s, u"a\\nb"_s }});

    QTest::newRow("unquoted-backslash-drops-backslash")
        << u"K=foo\\nbar\n"_s
        << map({{ u"K"_s, u"foonbar"_s }});

    QTest::newRow("unquoted-escaped-space")
        << u"K=foo\\ bar\n"_s
        << map({{ u"K"_s, u"foo bar"_s }});

    QTest::newRow("line-continuation-unquoted")
        << u"K=foo\\\nbar\n"_s
        << map({{ u"K"_s, u"foobar"_s }});

    QTest::newRow("line-continuation-double-quoted")
        << u"K=\"foo\\\nbar\"\n"_s
        << map({{ u"K"_s, u"foobar"_s }});

    QTest::newRow("multi-line-single-quoted")
        << u"K='hello\nworld'\n"_s
        << map({{ u"K"_s, u"hello\nworld"_s }});

    QTest::newRow("multi-line-double-quoted")
        << u"K=\"hello\nworld\"\n"_s
        << map({{ u"K"_s, u"hello\nworld"_s }});

    QTest::newRow("mixed-quoted-and-unquoted-concat")
        << u"K=\"hello\" world\n"_s
        // After closing quote, PRE_VALUE skips whitespace, then 'w' enters
        // VALUE state and accumulates onto the same value.
        << map({{ u"K"_s, u"helloworld"_s }});

    QTest::newRow("last-key-wins-on-duplicate")
        << u"K=a\nK=b\n"_s
        << map({{ u"K"_s, u"b"_s }});

    QTest::newRow("unterminated-double-quote-at-eof")
        << u"K=\"abc"_s
        << map({{ u"K"_s, u"abc"_s }});

    QTest::newRow("unterminated-single-quote-at-eof")
        << u"K='abc"_s
        << map({{ u"K"_s, u"abc"_s }});

    QTest::newRow("crlf-line-endings")
        << u"A=1\r\nB=2\r\n"_s
        << map({{ u"A"_s, u"1"_s }, { u"B"_s, u"2"_s }});

    QTest::newRow("malformed-line-without-equals")
        << u"GARBAGE\nA=1\n"_s
        << map({{ u"A"_s, u"1"_s }});

    QTest::newRow("backslash-in-comment-does-not-continue")
        // systemd v254+: '\<newline>' inside a comment does NOT continue the
        // comment to the next line - the next line parses as KEY=VALUE.
        << u"# foo \\\nA=1\n"_s
        << map({{ u"A"_s, u"1"_s }});
}

void tst_Systemd::parseEnvironmentFile()
{
    QFETCH(QString, contents);
    QFETCH(StringMap, expected);

    const auto actual = Systemd::parseEnvironmentFile(contents);
    QCOMPARE(actual, expected);
}

QTEST_APPLESS_MAIN(tst_Systemd)

#include "tst_systemd.moc"
