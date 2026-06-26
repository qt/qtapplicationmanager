// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QTest>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifndef F_GET_SEALS    // not always exposed by <fcntl.h>, depending on _GNU_SOURCE / glibc version
#  define F_GET_SEALS  (1024 + 10)
#  define F_SEAL_WRITE 0x0008
#endif

#include <QtEndian>
#include <QFile>

#include "utilities.h"
#include "exception.h"
#include "logging.h"
#include "systemd.h"
#if defined(QT_BUILD_INTERNAL)
#  include <QtAppManCommon/private/systemd_p.h>
#  include "unix-utilities.h"
#endif

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
    void extraJournalFields();
    void journalSend();

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

    // like read(), but also extracts a single SCM_RIGHTS file descriptor, if one was sent
    QByteArray readWithFd(int *outFd)
    {
        if (outFd)
            *outFd = -1;
        QByteArray buffer;
        buffer.resize(1024);
        struct ::iovec iov { buffer.data(), size_t(buffer.size()) };
        union {
            char buf[CMSG_SPACE(sizeof(int))];
            struct ::cmsghdr align;
        } control;
        struct ::msghdr mh {};
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        mh.msg_control = control.buf;
        mh.msg_controllen = sizeof(control.buf);

        ssize_t n = ::recvmsg(m_fd, &mh, 0);
        for (auto *cmsg = CMSG_FIRSTHDR(&mh); cmsg; cmsg = CMSG_NXTHDR(&mh, cmsg)) {
            if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SCM_RIGHTS)) {
                ::memcpy(outFd, CMSG_DATA(cmsg), sizeof(int));
                break;
            }
        }
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

void tst_Systemd::extraJournalFields()
{
    using Fields = QMap<QByteArray, QByteArray>;

    // invalid field names and values are rejected
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ "", "v" }}));            // empty name
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ QByteArray("\xff\xfe"), "v" }})); // invalid UTF-8 name
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ "A=B", "v" }}));         // '=' in name
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ "A\tB", "v" }}));        // control char in name
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ "KEY", "a\tb" }}));      // control char in value
    QVERIFY_THROWS_EXCEPTION(Exception, m_sd->setExtraJournalFields(Fields {{ "KEY", QByteArray("\xff\xfe") }})); // invalid UTF-8 value

    // a valid set is stored and reported back verbatim (a newline in a value is allowed)
    const Fields fields {
        { "FIELD_A", "value-a" },
        { "FIELD_B", "line1\nline2" }
    };
    QVERIFY_THROWS_NO_EXCEPTION(m_sd->setExtraJournalFields(fields));
    QCOMPARE(m_sd->extraJournalFields(), fields);

    // clearing works
    QVERIFY_THROWS_NO_EXCEPTION(m_sd->setExtraJournalFields({ }));
    QCOMPARE(m_sd->extraJournalFields(), Fields { });
}

void tst_Systemd::journalSend()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("This test requires a developer-build");
#else
    // Redirect logToJournal() onto a socket we control, so we can inspect the complete datagram
    // that would otherwise go to /run/systemd/journal/socket.
    DatagramSocket journal("@qtam-systemd-journal-test-socket");
    QVERIFY(journal.create());

    QVERIFY(SystemdPrivate::setJournalSocketPathForTesting(journal.name()));
    const auto resetSocket = qScopeGuard([]() { SystemdPrivate::setJournalSocketPathForTesting({ }); });

    // a path that does not fit into sockaddr_un::sun_path is rejected without changing anything
    QVERIFY(!SystemdPrivate::setJournalSocketPathForTesting(QByteArray(200, 'x')));

    Logging::setApplicationId("my.app");
    const auto resetAppId = qScopeGuard([]() { Logging::setApplicationId({ }); });

    QMessageLogContext context("some/file.cpp", 42, "myFunction", "my.category");

    // Parse a complete datagram according to the journald native protocol, making no assumption
    // about field order (the spec explicitly allows any order):
    // https://systemd.io/JOURNAL_NATIVE_PROTOCOL/
    //   - plain field:  "KEY=value\n"
    //   - binary field: "KEY\n" + 8-byte little-endian unsigned value size + value + "\n"
    // For each key we record both its value and whether the binary form was used, so the caller
    // can verify the spec rule "use the binary form if (and only if) the value contains a newline".
    struct Field { QByteArray value; bool binary; };
    auto parseJournal = [](const QByteArray &dgram) -> QMap<QByteArray, Field> {
        QMap<QByteArray, Field> fields;
        qsizetype pos = 0;
        while (pos < dgram.size()) {
            const qsizetype nl = dgram.indexOf('\n', pos);
            if (nl < 0)
                break;
            const qsizetype eq = dgram.indexOf('=', pos);
            if ((eq >= 0) && (eq < nl)) {
                // plain "KEY=value\n"
                const QByteArray key = dgram.mid(pos, eq - pos);
                fields[key] = { dgram.mid(eq + 1, nl - eq - 1), false };
                pos = nl + 1;
            } else {
                // binary "KEY\n<8-byte LE size>value\n"
                const QByteArray key = dgram.mid(pos, nl - pos);
                const quint64 len = qFromLittleEndian<quint64>(dgram.constData() + nl + 1);
                const qsizetype valuePos = nl + 1 + sizeof(quint64);
                fields[key] = { dgram.mid(valuePos, qsizetype(len)), true };
                pos = valuePos + qsizetype(len) + 1; // skip value and trailing '\n'
            }
        }
        return fields;
    };

    // single-line message -> plain "MESSAGE=" form, plus all the metadata fields
    {
        QByteArray b;
        QVERIFY(m_sd->logToJournal(QtWarningMsg, context, u"hello journal"_s, b));

        const auto fields = parseJournal(journal.read());
        QCOMPARE(fields.value("PRIORITY").value, "4");     // QtWarningMsg
        QCOMPARE(fields.value("QT_CATEGORY").value, "my.category");
        QCOMPARE(fields.value("QT_AM_APPID").value, "my.app");
        QCOMPARE(fields.value("CODE_FILE").value, "some/file.cpp");
        QCOMPARE(fields.value("CODE_LINE").value, "42");
        QCOMPARE(fields.value("CODE_FUNC").value, "myFunction");
        QVERIFY2(!fields.value("MESSAGE").binary, "a newline-free value must use the plain form");
        QCOMPARE(fields.value("MESSAGE").value, "[my.app | my.category] hello journal");
    }

    // multi-line message -> must use the binary-framed form
    {
        QByteArray b;
        QVERIFY(m_sd->logToJournal(QtInfoMsg, context, u"line one\nline two"_s, b));

        const auto fields = parseJournal(journal.read());
        QCOMPARE(fields.value("PRIORITY").value, "6");     // QtInfoMsg
        QVERIFY2(fields.value("MESSAGE").binary, "a value containing a newline must use the binary form");
        QCOMPARE(fields.value("MESSAGE").value, "[my.app | my.category] line one\nline two");
    }

    // A message larger than the journal socket's send buffer makes the initial sendmsg() fail with
    // EMSGSIZE, which triggers the memfd "Plan B" path that transfers the data as a sealed file
    // descriptor instead of an inline datagram. The buffer is kernel-tunable, so query its actual
    // size rather than assuming the 8 MB default: a MESSAGE as large as the whole send buffer can
    // never fit in a single datagram once the field framing is added.
    {
        const int sndbuf = []() {
            // Reproduce, on a throwaway socket, the SO_SNDBUF negotiation that logToJournal() runs
            // on its journal socket (see the static lambda in systemd.cpp). The result is a
            // combination of socket type, the requested size and the (test-lifetime-stable)
            // net.core.wmem_* sysctls, so an identically configured socket reports the identical
            // buffer size. Returns -1 on error.
            // Note: Linux returns twice the value set via SO_SNDBUF.

            Unix::Fd fd { ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0) };
            if (!fd)
                return -1;
            int value = 0;
            ::socklen_t valueLen = sizeof(value);
            if ((::getsockopt(fd.get(), SOL_SOCKET, SO_SNDBUF, &value, &valueLen) < 0)
                    || (valueLen != sizeof(value))) {
                return -1;
            }
            const int bufSize = 8 * 1024 * 1024; // 8MB, mirroring logToJournal()
            if (value < (2 * bufSize)) {
                value = bufSize;
                (void) ::setsockopt(fd.get(), SOL_SOCKET, SO_SNDBUF, &value, sizeof(value));
                valueLen = sizeof(value);
                if ((::getsockopt(fd.get(), SOL_SOCKET, SO_SNDBUF, &value, &valueLen) < 0)
                        || (valueLen != sizeof(value))) {
                    value = -1;
                }
            }
            return value;
        }();

        QVERIFY(sndbuf > 0);
        const QString huge(sndbuf, u'x');
        QByteArray b;
        QVERIFY(m_sd->logToJournal(QtCriticalMsg, context, huge, b));

        // the payload now arrives out-of-band via an fd, so the datagram itself carries no data
        int fd = -1;
        const QByteArray fdDgram = journal.readWithFd(&fd);
        QVERIFY(fdDgram.isEmpty());
        QVERIFY(fd >= 0);

        // the fd must be a sealed memfd whose contents are the same journal entry that would
        // otherwise have been the datagram payload (spec: "identical to ... the payload")
        const int seals = ::fcntl(fd, F_GET_SEALS);
        QVERIFY(seals >= 0);
        QVERIFY(seals & F_SEAL_WRITE);
        QFile memfd;
        QVERIFY(memfd.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle));
        QVERIFY(memfd.seek(0)); // the producer left the offset at the end after writing
        const QByteArray contents = memfd.readAll();
        QVERIFY(!contents.isEmpty());
        const auto fields = parseJournal(contents);
        QCOMPARE(fields.value("PRIORITY").value, "2");     // QtCriticalMsg
        QCOMPARE(fields.value("QT_CATEGORY").value, "my.category");
        QCOMPARE(fields.value("QT_AM_APPID").value, "my.app");
        QVERIFY2(!fields.value("MESSAGE").binary, "a newline-free value must use the plain form");
        QCOMPARE(fields.value("MESSAGE").value, "[my.app | my.category] " + huge.toUtf8());
    }

    // extra journal fields are encoded into every entry, using the same plain/binary rule, and a
    // user-supplied SYSLOG_IDENTIFIER suppresses the one logToJournal() would otherwise add itself
    {
        m_sd->setExtraJournalFields({
            { "FIELD_A", "value-a" },
            { "FIELD_B", "multi\nline" },
            { "SYSLOG_IDENTIFIER", "my-identifier" }
        });
        const auto resetFields = qScopeGuard([this]() { m_sd->setExtraJournalFields({ }); });

        QByteArray b;
        QVERIFY(m_sd->logToJournal(QtWarningMsg, context, u"with extra fields"_s, b));

        const QByteArray dgram = journal.read();
        const auto fields = parseJournal(dgram);
        QCOMPARE(fields.value("FIELD_A").value, "value-a");
        QVERIFY2(!fields.value("FIELD_A").binary, "a newline-free value must use the plain form");
        QCOMPARE(fields.value("FIELD_B").value, "multi\nline");
        QVERIFY2(fields.value("FIELD_B").binary, "a value with a newline must use the binary form");
        QCOMPARE(fields.value("SYSLOG_IDENTIFIER").value, "my-identifier");
        QCOMPARE(dgram.count("\nSYSLOG_IDENTIFIER="), 1); // not duplicated by logToJournal()
    }
#endif
}

QTEST_APPLESS_MAIN(tst_Systemd)

#include "tst_systemd.moc"
