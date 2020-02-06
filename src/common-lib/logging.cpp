/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#if defined(_WIN32) && (_WIN32_WINNT-0 < _WIN32_WINNT_VISTA)
// needed for QueryFullProcessImageNameW
#  undef WINVER
#  undef _WIN32_WINNT
#  define WINVER _WIN32_WINNT_VISTA
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <QThreadStorage>
#include <QAtomicInteger>
#include <QCoreApplication>

#include "global.h"
#include "logging.h"
#include "utilities.h"
#include "unixsignalhandler.h"

#include <stdio.h>

#if defined(Q_OS_WIN)
#  include <Windows.h>
#  include <io.h>
#  include <QOperatingSystemVersion>
#  include <QThread>

Q_CORE_EXPORT void qWinMsgHandler(QtMsgType t, const char* str);
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <signal.h>
#  if defined(Q_OS_MACOS)
#    include <libproc.h>
#  endif
#endif
#if defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <android/log.h>
#endif

#if defined(QT_GENIVIEXTRAS_LIB)
#  include <QtGeniviExtras/QtDlt>
#  include <QtGeniviExtras/QDltRegistration>
#else
#  define QDLT_LOGGING_CATEGORY(a,b,c,d) Q_LOGGING_CATEGORY(a,b)
#  define QDLT_FALLBACK_CATEGORY(a)
#  define QDLT_REGISTER_APPLICATION(a,b)
#endif

#ifndef QDLT_REGISTER_CONTEXT_ON_FIRST_USE
#  define QDLT_REGISTER_CONTEXT_ON_FIRST_USE(a)
#endif

QT_BEGIN_NAMESPACE_AM

#if defined(QT_GENIVIEXTRAS_LIB)
static const char *s_defaultSystemUiDltId = "QTAM";
static const char *s_defaultSystemUiDltDescription = "Qt Application Manager";
#endif

/*
//! [am-logging-categories]
\table
\header
    \li Category
    \li DLT Context ID
    \li Description
\row
    \li \c am.system
    \li \c SYS
    \li General system messages
\row
    \li \c am.installer
    \li \c INST
    \li Installer sub-system messages
\row
    \li \c am.graphics
    \li \c GRPH
    \li OpenGL/UI related messages
\row
    \li \c am.wayland.debug
    \li \c WAYL
    \li Wayland related messages
\row
    \li \c am.qml
    \li \c QML
    \li General QML related messages
\row
    \li \c am.runtime
    \li \c RT
    \li Runtime messages
\row
    \li \c am.runtime.qml
    \li \c QMRT
    \li QML runtime messages
\row
    \li \c am.qml.ipc
    \li \c QMIP
    \li QML IPC messages
\row
    \li \c am.notify
    \li \c NTFY
    \li Notification sub-system messages
\row
    \li \c am.deployment
    \li \c DPLM
    \li Deployment hints
\row
    \li \c am.intent
    \li \c INTN
    \li Intent sub-system messages
\row
    \li \c am.cache
    \li \c CACH
    \li Cache sub-system messages
\row
    \li \c general
    \li \c GEN
    \li Used for DLT logging only and enabled by default. Categories that have no context ID
        mapping in DLT default to \c GEN; this includes uncategorized logging.
\endtable
//! [am-logging-categories]
*/
QDLT_REGISTER_CONTEXT_ON_FIRST_USE(true)
QDLT_REGISTER_APPLICATION(s_defaultSystemUiDltId, s_defaultSystemUiDltDescription)
QDLT_LOGGING_CATEGORY(LogSystem, "am.system", "SYS", "General system messages")
QDLT_LOGGING_CATEGORY(LogInstaller, "am.installer", "INST", "Installer sub-system messages")
QDLT_LOGGING_CATEGORY(LogGraphics, "am.graphics", "GRPH", "OpenGL/UI related messages")
QDLT_LOGGING_CATEGORY(LogWaylandDebug, "am.wayland.debug", "WAYL", "Wayland related messages")
QDLT_LOGGING_CATEGORY(LogQml, "am.qml", "QML", "General QML related messages")
QDLT_LOGGING_CATEGORY(LogRuntime, "am.runtime", "RT", "Runtime messages")
QDLT_LOGGING_CATEGORY(LogQmlRuntime, "am.runtime.qml", "QMRT", "QML runtime messages")
QDLT_LOGGING_CATEGORY(LogQmlIpc, "am.qml.ipc", "QMIP", "QML IPC messages")
QDLT_LOGGING_CATEGORY(LogNotifications, "am.notify", "NTFY", "Notifications sub-system messages")
QDLT_LOGGING_CATEGORY(LogDeployment, "am.deployment", "DPLM", "Deployment hints")
QDLT_LOGGING_CATEGORY(LogIntents, "am.intent", "INTN", "Intents sub-system messages")
QDLT_LOGGING_CATEGORY(LogCache, "am.cache", "CACH", "Cache sub-system messages")
QDLT_LOGGING_CATEGORY(LogGeneral, "general", "GEN", "Messages without dedicated context ID (fallback)")
QDLT_FALLBACK_CATEGORY(LogGeneral)


struct DeferredMessage {
    DeferredMessage(QtMsgType _msgType, const QMessageLogContext &_context, const QString &_message);
    DeferredMessage(DeferredMessage &&other) Q_DECL_NOEXCEPT;
    ~DeferredMessage();

    QtMsgType msgType;
    int line;
    const char *file;
    const char *function;
    const char *category;
    const QString message;
};


bool Logging::s_dltEnabled =
#if defined(QT_GENIVIEXTRAS_LIB)
        true;
#else
        false;
#endif
bool Logging::s_messagePatternDefined = false;
bool Logging::s_useAMConsoleLogger = false;
QStringList Logging::s_rules;
QtMessageHandler Logging::s_defaultQtHandler = nullptr;
QByteArray Logging::s_applicationId = QByteArray();
QVariant Logging::s_useAMConsoleLoggerConfig = QVariant();

static std::vector<DeferredMessage> s_deferredMessages;

DeferredMessage::DeferredMessage(QtMsgType _msgType, const QMessageLogContext &_context, const QString &_message)
    : msgType(_msgType)
    , line(_context.line)
    , message(_message)
{
    file = qstrdup(_context.file);
    function = qstrdup(_context.function);
    category = qstrdup(_context.category);
}

DeferredMessage::DeferredMessage(DeferredMessage &&other) Q_DECL_NOEXCEPT
    : msgType(other.msgType)
    , line(other.line)
    , file(other.file)
    , function(other.function)
    , category(other.category)
    , message(other.message)
{
    other.file = nullptr;
    other.function = nullptr;
    other.category = nullptr;
}

DeferredMessage::~DeferredMessage()
{
    delete[] file;
    delete[] function;
    delete[] category;
}


static void colorLogToStderr(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
    if (msgType < QtDebugMsg || msgType > QtInfoMsg)
        msgType = QtCriticalMsg;

    // Try to re-use a 512 byte buffer per thread as much as possible to avoid allocations.
    static QThreadStorage<QByteArray> outBuffers;
    QByteArray &out = outBuffers.localData();
    if (out.capacity() > 512)
        out.clear();
    if (!out.capacity())
        out.reserve(512);
    out.resize(0);

    Console::init();
    int consoleWidth = Console::width();

    // Find out, if we have a valid code location and prepare the output strings
    const char *filename = nullptr;
    int filenameLength = 0;
    char linenumber[8];
    int linenumberLength = 0;

    if (context.line > 0 && context.file && context.file[0]) {
        QByteArray ba = QByteArray::fromRawData(context.file, int(qstrlen(context.file)));
        int pos = -1;
#if defined(Q_OS_WIN)
        pos = ba.lastIndexOf('\\');
#endif
        if (pos < 0)
            pos = ba.lastIndexOf('/');

        filename = context.file + pos + 1;
        filenameLength = ba.size() - pos - 1;

        linenumberLength = qsnprintf(linenumber, 8, "%d", qMin(context.line, 9999999));
        if (linenumberLength < 0 || linenumberLength >= int(sizeof(linenumber)))
            linenumberLength = 0;
        linenumber[linenumberLength] = 0;
    }

    static const char *msgTypeStr[] = { "DBG ", "WARN", "CRIT", "FATL", "INFO" };
    static const Console::Color msgTypeColor[] = { Console::Green, Console::Yellow, Console::Red,
                                                   Console::Magenta, Console::Blue };
    int categoryLength = int(qstrlen(context.category));
    QByteArray msg = message.toLocal8Bit(); // sadly this allocates, but there's no other way in Qt

    // the visible character length
    int outLength = 10 + int(qstrlen(context.category)) + msg.length(); // 10 = strlen("[XXXX | ] ")
    out.append('[');

    Console::colorize(out, Console::BrightFlag | msgTypeColor[msgType]);
    out.append(msgTypeStr[msgType], 4); // all msgTypeStrs are 4 characters
    Console::colorize(out, Console::Off);

    out.append(" | ");

    Console::colorize(out, Console::Red + qHash(QByteArray::fromRawData(context.category, categoryLength)) % 7);
    out.append(context.category, categoryLength);
    Console::colorize(out, Console::Off);

    const QByteArray appId = Logging::applicationId();
    if (!appId.isEmpty()) {
        out.append(" | ");

        Console::colorize(out, Console::Red + qHash(appId) % 7);
        out.append(appId);
        Console::colorize(out, Console::Off);

        outLength += (3 + appId.length()); // 3 == strlen(" | ")
    }

    out.append("] ");
    out.append(msg);

    if (filenameLength && linenumberLength) {
        int spacing = 1;
        if (consoleWidth > 0) {
            // right-align the location mark
            spacing = consoleWidth - outLength - linenumberLength - filenameLength - 4; // 4 == strlen(" [:]")

            // keep the location mark right-aligned, even if the message contains newlines
            int lastNewline = msg.lastIndexOf('\n');
            if (lastNewline >= 0)
                spacing += (outLength - msg.length() + lastNewline + 1);

            // keep the location mark right-aligned, even if the message is longer than the window width
            while (spacing < 0)
                spacing += consoleWidth;
        }
        out.append(spacing, ' ');
        out.append('[');

        Console::colorize(out, Console::Magenta);
        out.append(filename, filenameLength);
        Console::colorize(out, Console::Off);

        out.append(':');

        Console::colorize(out, Console::BrightFlag | Console::Magenta);
        out.append(linenumber, linenumberLength);
        Console::colorize(out, Console::Off);

        out.append("]\n");
    } else {
        out.append('\n');
    }

    if (consoleWidth <= 0) {
#if defined(Q_OS_WIN)
        // do not use QMutex to avoid possible recursions
        static CRITICAL_SECTION cs;
        static bool csInitialized = false;
        if (!csInitialized)
            InitializeCriticalSection(&cs);

        EnterCriticalSection(&cs);
        OutputDebugStringA(out.constData());
        LeaveCriticalSection(&cs);
        return;

#elif defined(Q_OS_ANDROID)
        android_LogPriority pri = ANDROID_LOG_DEBUG;
        switch (msgType) {
        default:
        case QtDebugMsg:    pri = ANDROID_LOG_DEBUG; break;
        case QtWarningMsg:  pri = ANDROID_LOG_WARN; break;
        case QtCriticalMsg: pri = ANDROID_LOG_ERROR; break;
        case QtFatalMsg:    pri = ANDROID_LOG_FATAL; break;
        };

        static QByteArray appName = QCoreApplication::applicationName().toLocal8Bit();

        __android_log_write(pri, appName.constData(), out.constData());
        return;
#endif
    }
    fputs(out.constData(), stderr);
}

void Logging::messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if defined(QT_GENIVIEXTRAS_LIB)
    if (s_dltEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
#endif
    if (Q_UNLIKELY(!s_useAMConsoleLogger))
        s_defaultQtHandler(msgType, context, message);
    else
        colorLogToStderr(msgType, context, message);
}

void Logging::deferredMessageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
    s_deferredMessages.emplace_back(msgType, context, message);
}

void Logging::initialize()
{
    initialize(0, nullptr);
}

void Logging::initialize(int argc, const char * const *argv)
{
    bool instantLogging = false;
    if (argc > 0 && argv) {
        for (int i = 1; i < argc; ++i) {
            if (strcmp("--no-dlt-logging", argv[i]) == 0)
                Logging::setDltEnabled(false);
            else if (strcmp("--log-instant", argv[i]) == 0)
                instantLogging = true;
        }
    }

    s_messagePatternDefined = qEnvironmentVariableIsSet("QT_MESSAGE_PATTERN");
    s_defaultQtHandler = instantLogging ? qInstallMessageHandler(messageHandler)
                                        : qInstallMessageHandler(deferredMessageHandler);
}

QStringList Logging::filterRules()
{
    return s_rules;
}

void Logging::setFilterRules(const QStringList &rules)
{
    s_rules = rules;
    QString rulesStr = rules.join(qL1C('\n'));
#if defined(QT_GENIVIEXTRAS_LIB)
    if (s_dltEnabled)
        rulesStr += qSL("\ngeneral=true");
#endif
    QLoggingCategory::setFilterRules(rulesStr);
}

void Logging::setMessagePattern(const QString &pattern)
{
    if (!pattern.isEmpty() && !s_messagePatternDefined) {
        qputenv("QT_MESSAGE_PATTERN", pattern.toLocal8Bit()); // pass on to application processes, however for the
        qSetMessagePattern(pattern);  // System UI this might be too late, so set pattern explicitly here.
        s_messagePatternDefined = true;
    }
}

QVariant Logging::useAMConsoleLogger()
{
    return s_useAMConsoleLoggerConfig;
}

void Logging::useAMConsoleLogger(const QVariant &config)
{
    s_useAMConsoleLoggerConfig = config;
    if (s_useAMConsoleLoggerConfig.userType() == QMetaType::Bool)
        s_useAMConsoleLogger = s_useAMConsoleLoggerConfig.toBool();
    else
        s_useAMConsoleLogger = !s_messagePatternDefined;
}

void Logging::completeSetup()
{
    for (const DeferredMessage &msg : s_deferredMessages) {
        QLoggingCategory cat(msg.category);
        if (cat.isEnabled(msg.msgType)) {
            QMessageLogContext context(msg.file, msg.line, msg.function, msg.category);
            messageHandler(msg.msgType, context, msg.message);
        }
    }

    std::vector<DeferredMessage>().swap(s_deferredMessages);
    qInstallMessageHandler(messageHandler);
}

QByteArray Logging::applicationId()
{
    return s_applicationId;
}

void Logging::setApplicationId(const QByteArray &appId)
{
    s_applicationId = appId;
}

bool Logging::deferredMessages()
{
    return !s_deferredMessages.empty();
}

bool Logging::isDltEnabled()
{
    return s_dltEnabled;
}

void Logging::setDltEnabled(bool enabled)
{
    s_dltEnabled = enabled;
}

void Logging::registerUnregisteredDltContexts()
{
#ifdef AM_GENIVIEXTRAS_LAZY_INIT
    if (s_dltEnabled)
        globalDltRegistration()->registerUnregisteredContexts();
#endif
}

void Logging::setSystemUiDltId(const QByteArray &dltAppId, const QByteArray &dltAppDescription)
{
#if defined(QT_GENIVIEXTRAS_LIB)
    if (s_dltEnabled) {
        const QByteArray id = dltAppId.isEmpty() ? QByteArray(s_defaultSystemUiDltId) : dltAppId;
        const QByteArray description = dltAppDescription.isEmpty() ? QByteArray(s_defaultSystemUiDltDescription)
                                                                   : dltAppDescription;
        globalDltRegistration()->registerApplication(id, description);
    }
#else
    Q_UNUSED(dltAppId)
    Q_UNUSED(dltAppDescription)
#endif
}

void Logging::setDltApplicationId(const QByteArray &dltAppId, const QByteArray &dltAppDescription)
{
#if defined(QT_GENIVIEXTRAS_LIB)
    if (s_dltEnabled)
        globalDltRegistration()->registerApplication(dltAppId, dltAppDescription);
#else
    Q_UNUSED(dltAppId)
    Q_UNUSED(dltAppDescription)
#endif
}

void Logging::logToDlt(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if defined(QT_GENIVIEXTRAS_LIB)
    if (s_dltEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
#else
    Q_UNUSED(msgType)
    Q_UNUSED(context)
    Q_UNUSED(message)
#endif
}

void am_trace(QDebug)
{ }

bool Console::supportsAnsiColor = false;
bool Console::isRunningInQtCreator = false;
bool Console::hasConsoleWindow = false;
QAtomicInt Console::consoleWidthCached(0);

void Console::init()
{
    static bool once = false;
    if (!once) {
        once = true;

        enum { ColorAuto, ColorOff, ColorOn } forceColor = ColorAuto;
        const QByteArray forceColorOutput = qgetenv("AM_FORCE_COLOR_OUTPUT");
        if (forceColorOutput == "off" || forceColorOutput == "0")
            forceColor = ColorOff;
        else if (forceColorOutput == "on" || forceColorOutput == "1")
            forceColor = ColorOn;

#if defined(Q_OS_UNIX)
        if (::isatty(STDERR_FILENO)) {
            hasConsoleWindow = true;
            supportsAnsiColor = true;
        }

#elif defined(Q_OS_WIN)
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        if (h != INVALID_HANDLE_VALUE) {

            if (QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10) {
                // enable ANSI mode on Windows 10
                DWORD mode = 0;
                if (GetConsoleMode(h, &mode)) {
                    mode |= 0x04;
                    if (SetConsoleMode(h, mode)) {
                        supportsAnsiColor = true;
                        hasConsoleWindow = true;
                    }
                }
            }
        }
#endif

        qint64 pid = QCoreApplication::applicationPid();
        forever {
            pid = getParentPid(pid);
            if (pid <= 1)
                break;

#if defined(Q_OS_LINUX)
            static QString checkCreator = qSL("/proc/%1/exe");
            QFileInfo fi(checkCreator.arg(pid));
            if (fi.symLinkTarget().contains(qSL("qtcreator"))) {
                isRunningInQtCreator = true;
                break;
            }
#elif defined(Q_OS_MACOS)
            static char buffer[PROC_PIDPATHINFO_MAXSIZE + 1];
            int len = proc_pidpath(pid, buffer, sizeof(buffer) - 1);
            if ((len > 0) && QByteArray::fromRawData(buffer, len).contains("Qt Creator")) {
                isRunningInQtCreator = true;
                break;
            }
#elif defined(Q_OS_WIN)
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
            if (hProcess) {
                wchar_t exeName[1024] = { 0 };
                DWORD exeNameSize = sizeof(exeName) - 1;
                if (QueryFullProcessImageNameW(hProcess, 0, exeName, &exeNameSize)) {
                    if (QString::fromWCharArray(exeName, exeNameSize).contains(qSL("qtcreator.exe")))
                        isRunningInQtCreator = true;
                }
            }
#endif
        }

        if (forceColor != ColorAuto)
            supportsAnsiColor = (forceColor == ColorOn);
        else if (!supportsAnsiColor)
            supportsAnsiColor = isRunningInQtCreator;

#if defined(Q_OS_UNIX) && defined(SIGWINCH)
        UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler, SIGWINCH, [](int) {
            // we are in a signal handler, so we just clear the cached value in the atomic int
            consoleWidthCached = 0;
        });
#elif defined(Q_OS_WIN)
        class ConsoleThread : public QThread
        {
        public:
            ConsoleThread(QObject *parent)
                : QThread(parent)
            { }

            ~ConsoleThread()
            {
                terminate();
                wait();
            }

        protected:
            void run() override
            {
                HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
                DWORD mode = 0;
                if (!GetConsoleMode(h, &mode))
                    return;
                if (!SetConsoleMode(h, mode | ENABLE_WINDOW_INPUT))
                    return;

                INPUT_RECORD ir;
                DWORD irRead = 0;
                while (ReadConsoleInputW(h, &ir, 1, &irRead)) {
                    if ((irRead == 1) && (ir.EventType == WINDOW_BUFFER_SIZE_EVENT))
                        consoleWidthCached = 0;
                }
            }
        };
        (new ConsoleThread(qApp))->start();
#endif // Q_OS_WIN
    }
}

int Console::width()
{
    int consoleWidthCalculated = consoleWidthCached;

    if (consoleWidthCalculated <= 0) {
        if (hasConsoleWindow) {
#if defined(Q_OS_UNIX)
            struct ::winsize ws;
            if ((::ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
                consoleWidthCalculated = ws.ws_col;
#elif defined(Q_OS_WIN)
            HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(h, &csbi))
                consoleWidthCalculated = csbi.dwSize.X;
#endif
        }
        consoleWidthCached = consoleWidthCalculated;
    }
    if ((consoleWidthCalculated <= 0) && isRunningInQtCreator)
        return 120;
    else
        return consoleWidthCalculated;
}

QByteArray &Console::colorize(QByteArray &out, int ansiColor, bool forceNoColor)
{
    static const char *ansiColors[] = {
        "\x1b[1m",  // bright
        "\x1b[0m",  // off
        "\x1b[30m", // black
        "\x1b[31m", // red
        "\x1b[32m", // green
        "\x1b[33m", // yellow
        "\x1b[34m", // blue
        "\x1b[35m", // magenta
        "\x1b[36m", // cyan
        "\x1b[37m" // gray
    };

    if (forceNoColor || !supportsAnsiColor)
        return out;
    if (ansiColor & BrightFlag)
        out.append(ansiColors[0]);
    if (ansiColor != BrightFlag)
        out.append(ansiColors[1 + (ansiColor & ~BrightFlag)]);
    return out;
}

QT_END_NAMESPACE_AM
