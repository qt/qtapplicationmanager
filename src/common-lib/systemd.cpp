// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QRegularExpression>
#include <QtCore/QReadWriteLock>
#include <QtEndian>
#include <qplatformdefs.h>
#include <sys/types.h>
#include "systemd.h"
#include "systemd_p.h"
#include "exception.h"
#include "logging.h"
#include "unix-utilities.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE // for program_invocation_short_name
#  endif
#  include <QtNetwork/private/qnet_unix_p.h>
#  include <charconv>
#  include <cerrno>
#  include <sys/mman.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/syscall.h>
#  include <sys/uio.h>
#  include <sys/un.h>
#  include <fcntl.h>
#  include <unistd.h>

#  ifndef MFD_NOEXEC_SEAL
#    define MFD_NOEXEC_SEAL 8U
#  endif
#  ifndef MFD_EXEC
#    define MFD_EXEC 0x10U
#  endif
#endif

using namespace std::chrono_literals;

QT_BEGIN_NAMESPACE_AM

Systemd *Systemd::instance()
{
    static Systemd instance;
    return &instance;
}

Systemd::~Systemd()
{ }

Systemd::Systemd()
    : d(std::make_unique<SystemdPrivate>())
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    auto getAndUnset = [](const char *name) {
        auto var = qgetenv(name);
        qunsetenv(name);
        return var;
    };

    d->notifySocket  = getAndUnset("NOTIFY_SOCKET");
    d->watchdogUsec  = getAndUnset("WATCHDOG_USEC");
    d->watchdogPid   = getAndUnset("WATCHDOG_PID");
    d->listenFds     = getAndUnset("LISTEN_FDS");
    d->listenFdNames = getAndUnset("LISTEN_FDNAMES");
    d->listenPid     = getAndUnset("LISTEN_PID");
    d->journalStream = qgetenv("JOURNAL_STREAM");
#endif
}

static bool checkPid(const QByteArray &pidVar)
{
    if (!pidVar.isEmpty()) {
        qint64 pid = pidVar.toLongLong();
        if (!pid || (pid != QCoreApplication::applicationPid()))
            return false;
    }
    return true;
}

bool Systemd::notify(const QString &state)
{
    if (d->notifySocket.isEmpty())
        return false;

    try {
        QByteArray stateStr = state.toUtf8();
        if (stateStr.isEmpty())
            throw Exception("empty notify messages are not allowed");

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        // connect lazily, keep the connection open, but only try to connect once
        if (!d->notifySocketFd) {
            if (d->notifySocketTriedToConnect)
                return false;
            d->notifySocketTriedToConnect = true;

            auto socketPath = d->notifySocket;

            if ((socketPath.at(0) != '@') && (socketPath.at(0) != '/'))
                throw Exception("invalid socket address: %1").arg(socketPath);

            // QLocalSocket cannot send datagrams and systemd does not allow streams...
            union {
                struct ::sockaddr sa;
                struct ::sockaddr_un sun;
            } socketAddr;
            ::memset(&socketAddr, 0, sizeof(socketAddr));
            socketAddr.sun.sun_family = AF_UNIX;

            if (socketPath.size() >= qsizetype(sizeof(socketAddr.sun.sun_path)))
                throw Exception("socket path too long: %1").arg(socketPath);
            ::memcpy(socketAddr.sun.sun_path, socketPath.constData(), socketPath.size());
            if (socketPath.at(0) == u'@') // abstract socket
                socketAddr.sun.sun_path[0] = 0;

            int fd = qt_safe_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (fd < 0)
                throw Exception(errno, "cannot create DGRAM socket");

            if (::connect(fd, &socketAddr.sa, offsetof(struct sockaddr_un, sun_path) + socketPath.size()) != 0) {
                qt_safe_close(fd);
                throw Exception(errno, "cannot connect to socket at %1").arg(socketPath);
            }
            d->notifySocketFd.reset(fd);
        }

        if (qt_safe_write(d->notifySocketFd.get(), stateStr.constData(), stateStr.size()) != stateStr.size())
            throw Exception(errno, "failed to send notify string");
#else
        Q_ASSERT(false);
#endif
        return true;
    } catch (const Exception &e) {
        qCWarning(LogSystem).noquote() << "Systemd notify:" << e.errorString();
        return false;
    }
}

std::optional<std::chrono::milliseconds> Systemd::watchdogTimeout(bool ignorePid)
{
    if (d->watchdogUsec.isEmpty())
        return { };

    if (!ignorePid && !checkPid(d->watchdogPid))
        return { };

    auto msecs = std::chrono::milliseconds(d->watchdogUsec.toULongLong() / 1000);
    if (msecs <= 1ms) // this needs to be > 0, when divided by 2
        return { };
    return msecs;
}

QMap<int, QString> Systemd::listenFds(const QRegularExpression &nameRx, bool ignorePid)
{
    if (d->listenFds.isEmpty())
        return { };

    if (!ignorePid && !checkPid(d->listenPid))
        return { };

    int fdCount = d->listenFds.toInt();
    if (fdCount <= 0)
        return { };

    const auto names = QString::fromLocal8Bit(d->listenFdNames).split(u':');

    if (names.size() != fdCount) {
        qCWarning(LogSystem).noquote() << "Systemd listen FDs count does not match names count";
        return { };
    }

    QMap<int, QString> result;
    for (int i = 0; i < fdCount; ++i) {
        if (nameRx.match(names[i]).hasMatch())
            result.insert(i + 3 /* fds start at 3 */, names[i]);
    }
    return result;
}

bool Systemd::canLogToJournal() const
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    static const bool result = [this] {
        QByteArrayView js(d->journalStream);
        if (qsizetype pos = js.indexOf(':'); pos > 0) {
            bool devOk, inoOk;
            dev_t dev = static_cast<dev_t>(js.left(pos).toULongLong(&devOk));
            ino_t ino = static_cast<ino_t>(js.mid(pos + 1).toULongLong(&inoOk));
            if (devOk && inoOk) {
                struct ::stat statStdErr;
                if (::fstat(STDERR_FILENO, &statStdErr) == 0) {
                    if ((statStdErr.st_dev == dev) && (statStdErr.st_ino == ino))
                        return true;
                }
            }
        }
        return false;
    }();
    return result;
#else
    return false;
#endif
}

#if defined(QT_BUILD_INTERNAL) && defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
QByteArray SystemdPrivate::s_journalSocketPathForTesting;

bool SystemdPrivate::setJournalSocketPathForTesting(const QByteArray &path)
{
    if (path.size() >= qsizetype(sizeof(sockaddr_un::sun_path)))
        return false;
    s_journalSocketPathForTesting = path;
    return true;
}
#endif

bool Systemd::logToJournal(QtMsgType msgType, const QMessageLogContext &context,
                           const QString &message, QByteArray &b)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Instead of pulling in a libsystemd dependency, we use the native journald protocol:
    // https://systemd.io/JOURNAL_NATIVE_PROTOCOL/
    // Combined with our logBuffers mechanism from logging.cpp, this handler is alloc-free and
    // async-signal-safe for any reasonable sized message.

    const char priority = [=]() {
        switch (msgType) {
        case QtDebugMsg:    return '7';
        case QtInfoMsg:     return '6';
        case QtWarningMsg:  return '4';
        default:
        case QtCriticalMsg: return '2';
        case QtFatalMsg:    return '1';
        }
    }();

    const QByteArray appId = Logging::applicationId();
    std::array<char, 11> lineBuf;
    std::array<char, 32> priAndTid { "PRIORITY=0\nTID=" };
    priAndTid.at(9) = priority;
    const auto [endPtr, error] = std::to_chars(priAndTid.begin() + 15, priAndTid.end(),
                                               (pid_t) ::syscall(SYS_gettid));
    const size_t priAndTidLen = (error == std::errc()) ? (endPtr - priAndTid.begin()) : 10;

    // We are using scatter/gather IO to send the message in one datagram without allocations
    // and with minimal copying.
    // For efficiency, the required trailing new-line is always pre-pended to the next field.

    std::array<struct ::iovec, 15> iov {{
        { (void *) priAndTid.data(), (size_t) priAndTidLen },
        { (void *) "\nQT_CATEGORY=", 13 },
        { (void *) context.category, qstrlen(context.category) },
    }};
    size_t iovLen = 3;

    if (!appId.isEmpty()) {
        iov.at(iovLen++) = { (void *) "\nQT_AM_APPID=", 13 };
        iov.at(iovLen++) = { (void *) appId.constData(), (size_t) appId.size() };
    }
    if (context.file && context.file[0]) {
        iov.at(iovLen++) = { (void *) "\nCODE_FILE=", 11 };
        iov.at(iovLen++) = { (void *) context.file, (size_t) qstrlen(context.file) };
    }
    if (context.line > 0) {
        auto [endPtr, error] = std::to_chars(lineBuf.begin(), lineBuf.end(), context.line);
        if (error == std::errc()) {
            iov.at(iovLen++) = { (void *) "\nCODE_LINE=", 11 };
            iov.at(iovLen++) = { (void *) lineBuf.data(), (size_t) (endPtr - lineBuf.data()) };
        }
    }
    if (context.function && context.function[0]) {
        iov.at(iovLen++) = { (void *) "\nCODE_FUNC=", 11 };
        iov.at(iovLen++) = { (void *) context.function, (size_t) qstrlen(context.function) };
    }

    bool hasSyslogIdentifier = false;
    if (d->extraJournalFieldsLock.tryLockForRead()) {
        // It's better to skip the extra fields than to block logging. This can only happen if
        // someone is calling setExtraJournalFields() from a different thread, but you should do
        // this only once in the startup phase anyway.
        if (!d->extraJournalFieldsBuffer.isEmpty()) {
            iov.at(iovLen++) = { (void *) d->extraJournalFieldsBuffer.constData(),
                                (size_t) d->extraJournalFieldsBuffer.size() };
            hasSyslogIdentifier = d->extraJournalFieldsHasSyslogIdentifier;
        }
        d->extraJournalFieldsLock.unlock();
    }

    if (!hasSyslogIdentifier) {
        // libsystemd compatibility: add SYSLOG_IDENTIFIER if not already explicitly set
        // and make sure it contains only valid characters (see sd_journal_sendv())
        QByteArrayView name(program_invocation_short_name);
        bool ok = !name.isEmpty();
        for (char c : name) {
            if (((c >= 0x01) && (c <= 0x1f)) || (c == 0x7f) || (c == '\'') || (c == '"')) {
                ok = false;
                break;
            }
        }
        if (ok) {
            iov.at(iovLen++) = { (void *) "\nSYSLOG_IDENTIFIER=", 19 };
            iov.at(iovLen++) = { (void *) name.constData(), (size_t) name.size() };
        }
    }

    bool hasNewlines = message.contains(u'\n');
    b += (hasNewlines ? "\nMESSAGE\n12345678" : "\nMESSAGE=");
    qsizetype msgBegin = b.size();

    b += '[';
    if (!appId.isEmpty()) {
        b += appId;
        b += " | ";
    }
    b += context.category;
    b += "] ";

    // Don't use QString::toLocal8Bit() here, because it always allocates memory.
    // QStringEncoder together with op+=() on the other hand only allocates if necessary.
    b += QStringEncoder(QStringEncoder::System).encode(message);
    if (hasNewlines) {
        // overwrite the '12345678' placeholder with the length of the message
        qsizetype msgEnd = b.size();
        qToLittleEndian(qint64(msgEnd - msgBegin), b.data() + msgBegin - 8);
    }
    b += '\n';

    iov.at(iovLen++) = { (void *) b.constData(), (size_t) b.size() };

    // The target is normally the system journal, but auto-tests can redirect it elsewhere.
    static const ::sockaddr_un defaultSa {
        .sun_family = AF_UNIX,
#if defined(__GNUC__) && (__GNUC__ < 11) && !defined(__clang__) // gcc < 11 bug
        { .sun_path = "/run/systemd/journal/socket" }
#else
        .sun_path = "/run/systemd/journal/socket"
#endif
    };

    ::sockaddr_un sa = defaultSa;
    socklen_t saLen = (socklen_t) (offsetof(struct sockaddr_un, sun_path) + ::strlen(defaultSa.sun_path));

#if defined(QT_BUILD_INTERNAL)
    // auto-tests can redirect the journal datagrams onto a socket they control; the path is
    // guaranteed to fit into sun_path by setJournalSocketPathForTesting()
    if (Q_UNLIKELY(!SystemdPrivate::s_journalSocketPathForTesting.isEmpty())) {
        const QByteArray &p = SystemdPrivate::s_journalSocketPathForTesting;
        ::memset(&sa, 0, sizeof(sa));
        sa.sun_family = AF_UNIX;
        ::memcpy(sa.sun_path, p.constData(), p.size());
        if (p.at(0) == '@') // abstract socket
            sa.sun_path[0] = 0;
        saLen = (socklen_t) (offsetof(struct sockaddr_un, sun_path) + p.size());
    }
#endif

    struct ::msghdr mh = {
        .msg_name    = (struct ::sockaddr *) &sa,
        .msg_namelen = saLen,
        .msg_iov     = iov.data(),
        .msg_iovlen  = iovLen,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags   = 0,
    };

    static const int journalSocket = []() {
        // this part is a reimplementation of libsystemd code
        int s = qt_safe_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (s >= 0) {
            int bufSize = 8 * 1024 * 1024; // 8MB
            int value;
            ::socklen_t valueLen = sizeof(value);
            if ((::getsockopt(s, SOL_SOCKET, SO_SNDBUF, &value, &valueLen) < 0)
                || (valueLen != sizeof(value))
                || (value < (2 * bufSize))) {
                value = bufSize;
                valueLen = sizeof(value);
                (void) ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, &value, valueLen);
            }
        }
        return s;
    }();

    if (Q_UNLIKELY(journalSocket < 0)) // Only possible, if the kernel is out of socket handles
        return false;

    ssize_t result = qt_safe_sendmsg(journalSocket, &mh, MSG_NOSIGNAL);

    if (result >= 0)
        return true;

    if ((errno != EMSGSIZE) && (errno != ENOBUFS) && (errno != EAGAIN))
        return false; // unrecoverable error

    // Plan B: Use a memfd to send the message in case it was too large or the socket buffer was full.
    //         This part is a reimplementation of the libsystemd code.

    static auto memfd_create_wrapper = [](const char *name, unsigned int mode) {
        // This is a wrapper around memfd_create() that adds compatibility with older kernels (< 6.3)
        // where memfd_create() did not support the MFD_EXEC and MFD_NOEXEC_SEAL flags

        int mfd = ::memfd_create(name, mode);
        if ((mfd < 0) && (errno == EINVAL)) {
            auto modeCompat = mode & ~(MFD_EXEC | MFD_NOEXEC_SEAL);
            if (mode == modeCompat)
                return mfd;
            mfd = ::memfd_create(name, modeCompat);
        }
        return mfd;
    };
    int memFd = memfd_create_wrapper("journal-data", MFD_CLOEXEC | MFD_NOEXEC_SEAL | MFD_ALLOW_SEALING);
    if (memFd < 0)
        return false;
    auto cleanup = qScopeGuard([=]() { qt_safe_close(memFd); });

    if (::writev(memFd, iov.data(), iovLen) < 0)
        return false;
    if (::fcntl(memFd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) < 0)
        return false;

    // We are re-using mh here, but we switch from data (msg_iov) to sending an fd (msg_control)
    mh.msg_iov = nullptr;
    mh.msg_iovlen = 0;

    // There is no nice way to avoid this C macro hell that's need to send a single fd along
    union {
        uint8_t buf[CMSG_SPACE(sizeof(int))];
        struct ::cmsghdr cmsghdr;
    } control;

    mh.msg_control = &control;
    mh.msg_controllen = sizeof(control);

    auto cmsg = CMSG_FIRSTHDR(&mh);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    ::memcpy(CMSG_DATA(cmsg), &memFd, sizeof(int));

    return (::sendmsg(journalSocket, &mh, MSG_NOSIGNAL) >= 0);
#else
    Q_UNUSED(msgType)
    Q_UNUSED(context)
    Q_UNUSED(message)
    Q_UNUSED(b)
    return false;
#endif
}

QMap<QByteArray, QByteArray> Systemd::extraJournalFields()
{
    QReadLocker locker(&d->extraJournalFieldsLock);
    return d->extraJournalFields;
}

void Systemd::setExtraJournalFields(const QMap<QByteArray, QByteArray> &fields)
{
    // Convert to a single, readily encoded buffer that uses the same efficient "new-line first"
    // format that the logToJournal() method uses for all the other fields.

    bool hasSyslogIdentifier = false;
    QByteArray buffer;
    for (const auto &[k, v] : fields.asKeyValueRange()) {
        if (k.isEmpty())
            throw Exception("System Journal: field names must not be empty");
        if (!k.isValidUtf8())
            throw Exception("System Journal: field names must be valid UTF-8");
        for (const auto c : k) {
            if ((c < 0x20) || (c == 0x7f) || (c == '='))
                throw Exception("System Journal: field names must not contain control characters or '='");
        }

        if (!v.isValidUtf8()) // we do not support binary data at the moment
            throw Exception("System Journal: field values must be valid UTF-8");
        for (const auto c : v) {
            if (((c < 0x20) && (c != '\n')) || (c == 0x7f))
                throw Exception("System Journal: field values must not contain control characters");
        }

        if (v.contains('\n')) {
            buffer += '\n' + k + "\n12345678";
            auto begin = buffer.size();
            buffer += v;
            auto end = buffer.size();
            qToLittleEndian(qint64(end - begin), buffer.data() + begin - 8);
        } else {
            buffer += '\n' + k + '=' + v;
        }
        if (k == "SYSLOG_IDENTIFIER")
            hasSyslogIdentifier = true;
    }

    QWriteLocker locker(&d->extraJournalFieldsLock);
    d->extraJournalFieldsBuffer = buffer;
    d->extraJournalFields = fields;
    d->extraJournalFieldsHasSyslogIdentifier = hasSyslogIdentifier;
}

QMap<QString, QString> Systemd::parseEnvironmentFile(const QString &contents)
{
    // Faithful Qt port of systemd's parse_env_file_internal (src/basic/env-file.c).

    enum class State : quint8 {
        PreKey,
        Key,
        PreValue,
        Value,
        ValueEscape,
        SingleQuoteValue,
        DoubleQuoteValue,
        DoubleQuoteValueEscape,
        Comment,
    };

    State state = State::PreKey;
    QString key;
    QString value;
    qsizetype lastKeyWs = -1;    // first index of trailing whitespace run in key
    qsizetype lastValueWs = -1;  // same, for value (tracked only in Value state)

    auto isNewline = [](QChar c) { return (c == u'\n') || (c == u'\r'); };
    auto isWs      = [&](QChar c) { return (c == u' ') || (c == u'\t') || isNewline(c); };
    auto isComment = [](QChar c) { return (c == u'#') || (c == u';'); };
    auto needsEsc  = [](QChar c) { return (c == u'"') || (c == u'\\') || (c == u'`') || (c == u'$'); };

    QMap<QString, QString> out;

    auto commit = [&]() {
        if (lastKeyWs >= 0)
            key.truncate(lastKeyWs);
        if ((state == State::Value) && (lastValueWs >= 0))
            value.truncate(lastValueWs);
        if (!key.isEmpty())
            out.insert(key, value);
        key.clear();
        value.clear();
        lastKeyWs = -1;
        lastValueWs = -1;
    };

    for (QChar c : contents) {
        switch (state) {
        case State::PreKey:
            if (isComment(c)) {
                state = State::Comment;
            } else if (!isWs(c)) {
                state = State::Key;
                key += c;
            }
            break;

        case State::Key:
            if (isNewline(c)) {
                state = State::PreKey;
                key.clear();
                lastKeyWs = -1;
            } else if (c == u'=') {
                state = State::PreValue;
            } else {
                if (!isWs(c))
                    lastKeyWs = -1;
                else if (lastKeyWs < 0)
                    lastKeyWs = key.size();
                key += c;
            }
            break;

        case State::PreValue:
            if (isNewline(c)) {
                commit();
                state = State::PreKey;
            } else if (c == u'\'') {
                state = State::SingleQuoteValue;
            } else if (c == u'"') {
                state = State::DoubleQuoteValue;
            } else if (c == u'\\') {
                state = State::ValueEscape;
            } else if (!isWs(c)) {
                state = State::Value;
                value += c;
            }
            break;

        case State::Value:
            if (isNewline(c)) {
                commit();
                state = State::PreKey;
            } else if (c == u'\\') {
                state = State::ValueEscape;
                lastValueWs = -1;
            } else {
                if (!isWs(c))
                    lastValueWs = -1;
                else if (lastValueWs < 0)
                    lastValueWs = value.size();
                value += c;
            }
            break;

        case State::ValueEscape:
            state = State::Value;
            // Escaped newlines are line continuations: drop both.
            if (!isNewline(c))
                value += c;
            break;

        case State::SingleQuoteValue:
            if (c == u'\'')
                state = State::PreValue;
            else
                value += c;
            break;

        case State::DoubleQuoteValue:
            if (c == u'"')
                state = State::PreValue;
            else if (c == u'\\')
                state = State::DoubleQuoteValueEscape;
            else
                value += c;
            break;

        case State::DoubleQuoteValueEscape:
            state = State::DoubleQuoteValue;
            if (needsEsc(c))
                value += c;                  // unescape: drop backslash, keep char
            else if (!isNewline(c))
                value += u'\\', value += c;  // keep backslash + char verbatim
            // escaped newline inside double quotes: drop both (continuation)
            break;

        case State::Comment:
            // Comments are entirely discarded, so '\' has no special meaning
            // here (unlike inside values where it can continue the line).
            // Only a newline ends the comment. This matches systemd v254+
            // behavior where '\<newline>' inside a comment does NOT continue
            // the comment - the next line is parsed as a real KEY=VALUE.
            if (isNewline(c))
                state = State::PreKey;
            break;
        }
    }

    // Commit any pending value at EOF (matches systemd's terminal-state branch,
    // including unterminated quoted values for files without trailing newline).
    switch (state) {
    case State::PreValue:
    case State::Value:
    case State::ValueEscape:
    case State::SingleQuoteValue:
    case State::DoubleQuoteValue:
    case State::DoubleQuoteValueEscape:
        commit();
        break;
    default:
        break;
    }

    return out;
}

QT_END_NAMESPACE_AM
