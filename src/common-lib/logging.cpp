// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QProcessEnvironment>
#include <QThreadStorage>
#include <QAtomicInteger>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QThread>

#include "global.h"
#include "logging.h"
#include "console.h"
#include "colorprint.h"
#include "qtappman_common-config_p.h"

#include <cstdio>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#elif defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <android/log.h>
#endif

#if QT_CONFIG(am_dltlogging)
#  include <QtDltLogging/QtDlt>
#  include <QtDltLogging/QDltRegistration>
#else
#  define QDLT_LOGGING_CATEGORY(a,b,c,d) Q_LOGGING_CATEGORY(a,b)
#  define QDLT_FALLBACK_CATEGORY(a)
#  define QDLT_REGISTER_APPLICATION(a,b)
#endif

#ifndef QDLT_REGISTER_CONTEXT_ON_FIRST_USE
#  define QDLT_REGISTER_CONTEXT_ON_FIRST_USE(a)
#endif

QT_BEGIN_NAMESPACE_AM

#if QT_CONFIG(am_dltlogging)
static constexpr const char *s_defaultSystemUiDltId = "QTAM";
static constexpr const char *s_defaultSystemUiDltDescription = "Qt Application Manager";
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
    \li QML messages from the application manager
\row
    \li \c am.runtime
    \li \c RT
    \li Runtime messages
\row
    \li \c am.quicklaunch
    \li \c QL
    \li Quick-Launcher messages
\row
    \li \c am.runtime.qml
    \li \c QMRT
    \li QML messages from the application managers's runtime
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
// AXIVION DISABLE Qt-NonPodGlobalStatic
QDLT_REGISTER_CONTEXT_ON_FIRST_USE(true)
QDLT_REGISTER_APPLICATION(s_defaultSystemUiDltId, s_defaultSystemUiDltDescription)
QDLT_LOGGING_CATEGORY(LogSystem, "am.system", "SYS", "General system messages")
QDLT_LOGGING_CATEGORY(LogInstaller, "am.installer", "INST", "Installer sub-system messages")
QDLT_LOGGING_CATEGORY(LogGraphics, "am.graphics", "GRPH", "OpenGL/UI related messages")
QDLT_LOGGING_CATEGORY(LogWaylandDebug, "am.wayland.debug", "WAYL", "Wayland related messages")
QDLT_LOGGING_CATEGORY(LogQml, "am.qml", "QML", "General QML related messages")
QDLT_LOGGING_CATEGORY(LogRuntime, "am.runtime", "RT", "Runtime messages")
QDLT_LOGGING_CATEGORY(LogQuickLaunch, "am.quicklaunch", "QL", "Quick-Launcher messages")
QDLT_LOGGING_CATEGORY(LogQmlRuntime, "am.runtime.qml", "QMRT", "QML runtime messages")
QDLT_LOGGING_CATEGORY(LogNotifications, "am.notify", "NTFY", "Notifications sub-system messages")
QDLT_LOGGING_CATEGORY(LogDeployment, "am.deployment", "DPLM", "Deployment hints")
QDLT_LOGGING_CATEGORY(LogIntents, "am.intent", "INTN", "Intents sub-system messages")
QDLT_LOGGING_CATEGORY(LogCache, "am.cache", "CACH", "Cache sub-system messages")
QDLT_LOGGING_CATEGORY(LogDBus, "am.dbus", "DBUS", "D-Bus related messages")
QDLT_LOGGING_CATEGORY(LogGeneral, "general", "GEN", "Messages without dedicated context ID (fallback)")
QDLT_FALLBACK_CATEGORY(LogGeneral)
// AXIVION ENABLE Qt-NonPodGlobalStatic

struct DeferredMessage
{
    DeferredMessage(QtMsgType _msgType, const QMessageLogContext &_context, const QString &_message);
    DeferredMessage(const DeferredMessage &copy) = delete;
    DeferredMessage(DeferredMessage &&other) noexcept;
    ~DeferredMessage();
    DeferredMessage &operator=(const DeferredMessage &copy) = delete;
    DeferredMessage &operator=(DeferredMessage &&move) = delete;

    QtMsgType msgType;
    int line;
    const char *file;
    const char *function;
    const char *category;
    const QString message;
};

struct LoggingGlobal
{
    bool dltEnabled = QT_CONFIG(am_dltlogging);
    bool messagePatternDefined = false;
    bool useAMConsoleLogger = false;
    bool noCustomLogging = false;
    QStringList rules;
    QtMessageHandler defaultQtHandler = nullptr;
    QByteArray applicationId;
    QVariant useAMConsoleLoggerConfig;
    QString dltLongMessageBehavior;

    QMutex deferredMessagesMutex;
    std::vector<DeferredMessage> deferredMessages;

    // As multiple threads may log at the same time, we need to decouple the output
    // buffers:

    // This many threads can concurrently print log messages
    // any more threads will have to wait until one of the other threads is done
    // (for performance reasons, this is capped at 32 in the current implementation with
    // logBufferAvailable being a 32 bit atomic integer)
    static constexpr int LogBufferCount = 3;

    // The initial size of each buffer. Keep in mind that we need to convert the actual
    // log message text from UTF16 to UTF8 in place, so this needs to be around triple
    // the size of the longest message to completely avoid allocations (see QStringEncoder).
    static constexpr int LogBufferSize = 1024;

    // The maximum size of any buffer. If a buffer got resized beyond this valud due to
    // a very long message text, it will be discarded and a new buffer will be allocated.
    static constexpr int LogBufferMaxSize = 2 * LogBufferSize;

    std::array<QByteArray, LogBufferCount> logBuffers;
    QAtomicInteger<quint32> logBufferAvailable { quint32(~0) };

    // make sure we don't exceed our availability bit field
    static_assert(LogBufferCount <= 32);

    uint acquireLogBuffer()
    {
        do {
            quint32 currentlyAvailable = logBufferAvailable;
            uint idx = qCountTrailingZeroBits(currentlyAvailable);
            if (idx >= LoggingGlobal::LogBufferCount) {
                QThread::usleep(5);
                continue;
            }
            quint32 reservation = currentlyAvailable ^ (1 << idx);
            if (!logBufferAvailable.testAndSetOrdered(currentlyAvailable, reservation))
                continue;

            return idx;
        } while (true);
    }

    void releaseLogBuffer(uint idx)
    {
        Q_ASSERT(idx < LoggingGlobal::LogBufferCount);
        Q_ASSERT((logBufferAvailable & (1 << idx)) == 0);
        logBufferAvailable ^= (1 << idx);
    }

    ~LoggingGlobal();
};

Q_GLOBAL_STATIC(LoggingGlobal, lg)

DeferredMessage::DeferredMessage(QtMsgType _msgType, const QMessageLogContext &_context, const QString &_message)
    : msgType(_msgType)
    , line(_context.line)
    , message(_message)
{
    file = qstrdup(_context.file);
    function = qstrdup(_context.function);
    category = qstrdup(_context.category);
}

DeferredMessage::DeferredMessage(DeferredMessage &&other) noexcept
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

    uint logBufferIndex = lg()->acquireLogBuffer();
    auto releaseLogBuffer = qScopeGuard([=] { lg()->releaseLogBuffer(logBufferIndex); });

    QByteArray &logBuffer = lg()->logBuffers[logBufferIndex];

    if (logBuffer.capacity() > LoggingGlobal::LogBufferMaxSize)
        logBuffer.clear();
    if (!logBuffer.capacity())
        logBuffer.reserve(LoggingGlobal::LogBufferSize);
    logBuffer.resize(0);

    int consoleWidth = Console::width();

    // Find out, if we have a valid code location and prepare the output strings
    const char *filename = nullptr;
    qsizetype filenameLength = 0;
    char linenumber[8];
    qsizetype linenumberLength = 0;

    if (context.line > 0 && context.file && context.file[0]) {
        QByteArrayView fileView { context.file };
        qsizetype pos = -1;
#if defined(Q_OS_WIN)
        pos = fileView.lastIndexOf('\\');
#endif
        if (pos < 0)
            pos = fileView.lastIndexOf('/');

        filename = context.file + pos + 1;
        filenameLength = fileView.size() - pos - 1;

        linenumberLength = qsnprintf(linenumber, 8, "%d", qMin(context.line, 9999999));
        if (linenumberLength < 0 || linenumberLength >= int(sizeof(linenumber)))
            linenumberLength = 0;
        linenumber[linenumberLength] = 0;
    }

    struct MsgType { const char *name; ColorPrint::Command color; };
    static const std::array<const MsgType, 5> msgTypes {{
        { "DBG ", ColorPrint::bgreen },
        { "WARN", ColorPrint::byellow },
        { "CRIT", ColorPrint::bred },
        { "FATL", ColorPrint::bmagenta },
        { "INFO", ColorPrint::bblue },
    }};

    qsizetype categoryLength = int(qstrlen(context.category));
    // the visible character length
    qsizetype outLength = 10 + categoryLength; // 10 = strlen("[XXXX | ] ")

    ColorPrint cprt(logBuffer, Console::stderrSupportsAnsiColor());

    cprt << "[" << msgTypes[msgType].color << msgTypes[msgType].name << ColorPrint::reset << " | "
         << ColorPrint::Command(ColorPrint::red + qHash(QByteArrayView { context.category, categoryLength }) % 7)
         << context.category << ColorPrint::reset;

    if (const QByteArray appId = Logging::applicationId(); !appId.isEmpty()) {
        cprt << " | " << ColorPrint::Command(ColorPrint::red + qHash(appId) % 7) << appId << ColorPrint::reset;
        outLength += (3 + appId.length()); // 3 == strlen(" | ")
    }

    cprt << "] ";

    qsizetype outSizeBeforeMessage = logBuffer.size();
    cprt << message;
    outLength += (logBuffer.size() - outSizeBeforeMessage);

    if (filenameLength && linenumberLength) {
        qsizetype spacing = 1;
        if (consoleWidth > 0) {
            // right-align the location mark
            spacing = consoleWidth - outLength - linenumberLength - filenameLength + 1 - 4; // 4 == strlen(" [:]")

            // keep the location mark right-aligned, even if the message contains newlines
            qsizetype lastNewline = logBuffer.lastIndexOf('\n');
            if (lastNewline >= 0)
                spacing += (outLength - (logBuffer.size() - lastNewline) + 1);

            if (Console::isRunningInQtCreator()) {
                // we cannot control line-breaks in Qt Creator's Application Output window, so we
                // just use 3 spaces to separate the location mark on long lines
                if (spacing < 0)
                    spacing = 3;
            } else {
                // on real terminals, we can keep the location mark right-aligned, even if the
                // message is longer than the window width
                while (spacing < 0)
                    spacing += consoleWidth;
            }
        }
        cprt << ColorPrint::repeat(spacing, ' ');
        cprt << '[' << ColorPrint::magenta << ColorPrint::subString(filename, filenameLength)
             << ColorPrint::reset << ':' << ColorPrint::bmagenta
             << ColorPrint::subString(linenumber, linenumberLength) << ColorPrint::reset << "]\n";
    } else {
        cprt << '\n';
    }

    if (consoleWidth <= 0) {
#if defined(Q_OS_WIN)
        OutputDebugStringA(logBuffer.constData());
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

        __android_log_write(pri, appName.constData(), logBuffer.constData());
        return;
#endif
    }
    fputs(logBuffer.constData(), stderr);
}

void Logging::messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if QT_CONFIG(am_dltlogging)
    if (lg()->dltEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
#endif
    if (Q_UNLIKELY(!lg()->useAMConsoleLogger))
        lg()->defaultQtHandler(msgType, context, message);
    else
        colorLogToStderr(msgType, context, message);
}

void Logging::deferredMessageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker lock(&lg()->deferredMessagesMutex);
    lg()->deferredMessages.emplace_back(msgType, context, message);
}

void Logging::initialize()
{
    initialize(0, nullptr);
}

void Logging::initialize(int argc, const char * const *argv)
{
    if (qEnvironmentVariableIntValue("AM_NO_CUSTOM_LOGGING") == 1) {
        lg()->noCustomLogging = true;
        lg()->dltEnabled = false;
        return;
    }

    if (qEnvironmentVariableIntValue("AM_NO_DLT_LOGGING") == 1)
        lg()->dltEnabled = false;

    bool instantLogging = false;
    if (argc > 0 && argv) {
        for (int i = 1; i < argc; ++i) {
            if (strcmp("--no-dlt-logging", argv[i]) == 0)
                lg()->dltEnabled = false;
            else if (strcmp("--log-instant", argv[i]) == 0)
                instantLogging = true;
        }
    }

    lg()->messagePatternDefined = qEnvironmentVariableIsSet("QT_MESSAGE_PATTERN");
    lg()->defaultQtHandler = instantLogging ? qInstallMessageHandler(messageHandler)
                                            : qInstallMessageHandler(deferredMessageHandler);
}

LoggingGlobal::~LoggingGlobal()
{
    // we do an unnecessary (but cheap) qInstallMessageHandler() at program exit, but this way
    // we cannot forget to dump the deferred messages whenever we exit
    Logging::completeSetup();

    // we are dead now, so make sure that anyone logging after this point will not crash the program
    if (defaultQtHandler)
        qInstallMessageHandler(defaultQtHandler);
}

QStringList Logging::filterRules()
{
    return lg()->rules;
}

void Logging::setFilterRules(const QStringList &rules)
{
    lg()->rules = rules;
    QString rulesStr = rules.join(u'\n');
    if (QT_CONFIG(am_dltlogging) && lg()->dltEnabled)
        rulesStr += u"\ngeneral=true";
    QLoggingCategory::setFilterRules(rulesStr);
}

void Logging::setMessagePattern(const QString &pattern)
{
    if (!pattern.isEmpty() && !lg()->messagePatternDefined && !lg()->noCustomLogging) {
        qputenv("QT_MESSAGE_PATTERN", pattern.toLocal8Bit()); // pass on to application processes, however for the
        qSetMessagePattern(pattern);  // System UI this might be too late, so set pattern explicitly here.
        lg()->messagePatternDefined = true;
    }
}

QVariant Logging::useAMConsoleLogger()
{
    return lg()->useAMConsoleLoggerConfig;
}

void Logging::setUseAMConsoleLogger(const QVariant &config)
{
    lg()->useAMConsoleLoggerConfig = config;
    if (lg()->useAMConsoleLoggerConfig.userType() == QMetaType::Bool)
        lg()->useAMConsoleLogger = lg()->useAMConsoleLoggerConfig.toBool();
    else
        lg()->useAMConsoleLogger = !lg()->messagePatternDefined;
}

void Logging::completeSetup()
{
    if (!lg()->noCustomLogging) {
        qInstallMessageHandler(messageHandler);

        QMutexLocker lock(&lg()->deferredMessagesMutex);
        for (const DeferredMessage &msg : lg()->deferredMessages) {
            QLoggingCategory cat(msg.category);
            if (cat.isEnabled(msg.msgType)) {
                QMessageLogContext context(msg.file, msg.line, msg.function, msg.category);
                messageHandler(msg.msgType, context, msg.message);
            }
        }
        std::vector<DeferredMessage>().swap(lg()->deferredMessages);
    }
}

QByteArray Logging::applicationId()
{
    return lg()->applicationId;
}

void Logging::setApplicationId(const QByteArray &appId)
{
    lg()->applicationId = appId;
}

bool Logging::hasDeferredMessages()
{
    QMutexLocker lock(&lg()->deferredMessagesMutex);
    return !lg()->deferredMessages.empty();
}

bool Logging::isDltEnabled()
{
    return lg()->dltEnabled;
}

void Logging::setDltEnabled(bool enabled)
{
    lg()->dltEnabled = enabled;
}

void Logging::registerUnregisteredDltContexts()
{
#if QT_CONFIG(am_dltlogging)
    if (lg()->dltEnabled)
        globalDltRegistration()->registerUnregisteredContexts();
#endif
}

void Logging::setSystemUiDltId(const QByteArray &dltAppId, const QByteArray &dltAppDescription)
{
#if QT_CONFIG(am_dltlogging)
    if (lg()->dltEnabled) {
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
#if QT_CONFIG(am_dltlogging)
    if (lg()->dltEnabled)
        globalDltRegistration()->registerApplication(dltAppId, dltAppDescription);
#else
    Q_UNUSED(dltAppId)
    Q_UNUSED(dltAppDescription)
#endif
}

QString Logging::dltLongMessageBehavior()
{
    return lg()->dltLongMessageBehavior;
}

void Logging::setDltLongMessageBehavior(const QString &behaviorString)
{
    if (!lg()->dltEnabled)
        return;

    lg()->dltLongMessageBehavior = behaviorString;

#if QT_CONFIG(am_dltlogging)
    QDltRegistration::LongMessageBehavior behavior = QDltRegistration::LongMessageBehavior::Truncate;
    if (behaviorString == u"split")
        behavior = QDltRegistration::LongMessageBehavior::Split;
    else if (behaviorString == u"pass")
        behavior = QDltRegistration::LongMessageBehavior::Pass;

    globalDltRegistration()->setLongMessageBehavior(behavior);
#endif
}

void Logging::logToDlt(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if QT_CONFIG(am_dltlogging)
    if (lg()->dltEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
#else
    Q_UNUSED(msgType)
    Q_UNUSED(context)
    Q_UNUSED(message)
#endif
}

QT_END_NAMESPACE_AM
