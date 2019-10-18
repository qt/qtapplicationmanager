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

#include <QThreadStorage>
#include <QAtomicInteger>

#include "global.h"
#include "logging.h"
#include "utilities.h"

#include <stdio.h>
#if defined(Q_OS_WIN)
Q_CORE_EXPORT void qWinMsgHandler(QtMsgType t, const char* str);
#  include <windows.h>
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
QDLT_LOGGING_CATEGORY(LogQmlRuntime, "am.runtime.qml", "QMRT", "QML runtime messages")
QDLT_LOGGING_CATEGORY(LogQmlIpc, "am.qml.ipc", "QMIP", "QML IPC messages")
QDLT_LOGGING_CATEGORY(LogNotifications, "am.notify", "NTFY", "Notifications sub-system messages")
QDLT_LOGGING_CATEGORY(LogDeployment, "am.deployment", "DPLM", "Deployment hints")
QDLT_LOGGING_CATEGORY(LogIntents, "am.intent", "INTN", "Intents sub-system messages")
QDLT_LOGGING_CATEGORY(LogCache, "am.cache", "CACH", "Cache sub-system messages")
QDLT_LOGGING_CATEGORY(LogGeneral, "general", "GEN", "Messages without dedicated context ID (fallback)")
QDLT_FALLBACK_CATEGORY(LogGeneral)


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

    int consoleWidth = -1;
    bool ansiColorSupport = false;
    bool runningInCreator = false;
    getOutputInformation(&ansiColorSupport, &runningInCreator, &consoleWidth);

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

    enum ConsoleColor { Off = 0, Black, Red, Green, Yellow, Blue, Magenta, Cyan, Gray, BrightFlag = 0x80 };

    // helper function to append ANSI color codes to a string
    static auto color = [ansiColorSupport](QByteArray &out, int consoleColor) -> void {
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

        if (!ansiColorSupport)
            return;
        if (consoleColor & BrightFlag)
            out.append(ansiColors[0]);
        out.append(ansiColors[1 + (consoleColor & ~BrightFlag)]);
    };

    static const char *msgTypeStr[] = { "DBG ", "WARN", "CRIT", "FATL", "INFO" };
    static const ConsoleColor msgTypeColor[] = { Green, Yellow, Red, Magenta, Blue };
    int categoryLength = int(qstrlen(context.category));
    QByteArray msg = message.toLocal8Bit(); // sadly this allocates, but there's no other way in Qt

    // the visible character length
    int outLength = 10 + int(qstrlen(context.category)) + msg.length(); // 10 = strlen("[XXXX | ] ")
    out.append('[');

    color(out, BrightFlag | msgTypeColor[msgType]);
    out.append(msgTypeStr[msgType], 4); // all msgTypeStrs are 4 characters
    color(out, Off);

    out.append(" | ");

    color(out, Red + qHash(QByteArray::fromRawData(context.category, categoryLength)) % 7);
    out.append(context.category, categoryLength);
    color(out, Off);

    const QByteArray appId = Logging::applicationId();
    if (!appId.isEmpty()) {
        out.append(" | ");

        color(out, Red + qHash(appId) % 7);
        out.append(appId);
        color(out, Off);

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

        color(out, Magenta);
        out.append(filename, filenameLength);
        color(out, Off);

        out.append(':');

        color(out, BrightFlag | Magenta);
        out.append(linenumber, linenumberLength);
        color(out, Off);

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

void Logging::initialize()
{
    initialize(0, nullptr);
}

void Logging::initialize(int argc, const char * const *argv)
{
    if (argc > 0 && argv) {
        for (int i = 1; i < argc; ++i) {
            if (strcmp("--no-dlt-logging", argv[i]) == 0) {
                Logging::setDltEnabled(false);
                break;
            }
        }
    }

    auto messageHandler = [](QtMsgType msgType, const QMessageLogContext &context, const QString &message) {
#if defined(QT_GENIVIEXTRAS_LIB)
        if (s_dltEnabled)
            QDltRegistration::messageHandler(msgType, context, message);
#endif
        if (Q_UNLIKELY(!s_useAMConsoleLogger))
            s_defaultQtHandler(msgType, context, message);
        else
            colorLogToStderr(msgType, context, message);
    };

    s_messagePatternDefined = qEnvironmentVariableIsSet("QT_MESSAGE_PATTERN");
    s_defaultQtHandler = qInstallMessageHandler(messageHandler);
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
    if (!pattern.isEmpty()) {
        if (!s_messagePatternDefined) {
            qputenv("QT_MESSAGE_PATTERN", pattern.toLocal8Bit());
            s_messagePatternDefined = true;
        }
    }
}

QVariant Logging::useAMConsoleLogger()
{
    return s_useAMConsoleLoggerConfig;
}

void Logging::useAMConsoleLogger(const QVariant &config)
{
    s_useAMConsoleLoggerConfig = config;
    if (!s_useAMConsoleLoggerConfig.isValid())
        s_useAMConsoleLogger = !s_messagePatternDefined;
    else
        s_useAMConsoleLogger = s_useAMConsoleLoggerConfig.toBool();
}

QByteArray Logging::applicationId()
{
    return s_applicationId;
}

void Logging::setApplicationId(const QByteArray &appId)
{
    s_applicationId = appId;
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

QT_END_NAMESPACE_AM
