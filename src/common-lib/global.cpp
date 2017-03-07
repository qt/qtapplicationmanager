/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QFile>
#include <QNetworkInterface>
#include <QThreadStorage>
#include <QAtomicInteger>

#include "global.h"
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

/*
//! [am-logging-categories]
\list
\li \c am.installer - Installer sub-system
\li \c am.notify - Notification sub-system
\li \c am.qml - QML messages
\li \c am.qml.ipc - QML IPC
\li \c am.runtime.qml - QML runtime
\li \c am.system - General system messages
\li \c am.wayland - Wayland sub-system
\li \c general - General messages not part of any ApplicationManager sub-system
\endlist
//! [am-logging-categories]
*/
QDLT_REGISTER_CONTEXT_ON_FIRST_USE(true)
QDLT_REGISTER_APPLICATION("PCAM", "Pelagicore Application-Manager")
QDLT_LOGGING_CATEGORY(LogSystem, "am.system", "SYS", "General system messages")
QDLT_LOGGING_CATEGORY(LogInstaller, "am.installer", "INST", "Installer sub-system")
QDLT_LOGGING_CATEGORY(LogWayland, "am.wayland", "WAYL", "Wayland sub-system")
QDLT_LOGGING_CATEGORY(LogQml, "am.qml", "QML", "QML messages")
QDLT_LOGGING_CATEGORY(LogNotifications, "am.notify", "NTFY", "Notification sub-system")
QDLT_LOGGING_CATEGORY(LogQmlRuntime, "am.runtime.qml", "QMRT", "QML runtime")
QDLT_LOGGING_CATEGORY(LogQmlIpc, "am.qml.ipc", "QMIP", "QML IPC")
QDLT_LOGGING_CATEGORY(LogGeneral, "general", "GEN", "General messages not part of any ApplicationManager sub-system")
QDLT_FALLBACK_CATEGORY(LogGeneral)


QByteArray colorLogApplicationId = QByteArray();
bool dltLoggingEnabled = true;

static void colorLogToStderr(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if defined(QT_GENIVIEXTRAS_LIB)
    if (dltLoggingEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
#endif

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
        QByteArray ba = QByteArray::fromRawData(context.file, strlen(context.file));
        int pos = -1;
#if defined(Q_OS_WIN)
        pos = ba.lastIndexOf('\\');
#endif
        if (pos < 0)
            pos = ba.lastIndexOf('/');
        if (pos >= 0) {
            filename = context.file + pos + 1;
            filenameLength = ba.size() - pos - 1;

            linenumberLength = qsnprintf(linenumber, 8, "%d", qMin(context.line, 9999999));
            if (linenumberLength < 0 || linenumberLength >= int(sizeof(linenumber)))
                linenumberLength = 0;
            linenumber[linenumberLength] = 0;
        }
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
    int categoryLength = strlen(context.category);
    QByteArray msg = message.toLocal8Bit(); // sadly this allocates, but there's no other way in Qt

    // the visible character length
    int outLength = 10 + strlen(context.category) + msg.length(); // 10 = strlen("[XXXX | ] ")
    out.append('[');

    color(out, BrightFlag | msgTypeColor[msgType]);
    out.append(msgTypeStr[msgType], 4); // all msgTypeStrs are 4 characters
    color(out, Off);

    out.append(" | ");

    color(out, Red + qHash(QByteArray::fromRawData(context.category, categoryLength)) % 7);
    out.append(context.category, categoryLength);
    color(out, Off);

    if (!colorLogApplicationId.isEmpty()) {
        out.append(" | ");

        color(out, Red + qHash(colorLogApplicationId) % 7);
        out.append(colorLogApplicationId);
        color(out, Off);

        outLength += (3 + colorLogApplicationId.length()); // 3 == strlen(" | ")
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
#if QT_VERSION < QT_VERSION_CHECK(5,7,0)
        // efficiently appending spaces without allocating is hard in Qt 5.6
        static const char spacingStr[] = "                                                                               ";
        Q_STATIC_ASSERT(sizeof(spacingStr) == 80);
        while (spacing > 0) {
            int spacingLen = qMin(spacing, int(sizeof(spacingStr)) - 1);
            out.append(spacingStr, spacingLen);
            spacing -= spacingLen;
        }
#else
        out.append(spacing, ' ');
#endif
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

        __android_log_print(pri, appName.constData(), out.constData());
        return;
#endif
    }
    fputs(out.constData(), stderr);
}

#if defined(QT_GENIVIEXTRAS_LIB)

static QtMessageHandler defaultQtMsgHandler;

static void compositeMsgHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
    if (dltLoggingEnabled)
        QDltRegistration::messageHandler(msgType, context, message);
    defaultQtMsgHandler(msgType, context, message);
}

#endif

void installMessageHandlers()
{
    if (Q_LIKELY(!qEnvironmentVariableIsSet("QT_MESSAGE_PATTERN")))
        qInstallMessageHandler(colorLogToStderr);
#if defined(QT_GENIVIEXTRAS_LIB)
    else
        defaultQtMsgHandler = qInstallMessageHandler(compositeMsgHandler);
#endif
}

QString hardwareId()
{
#if defined(AM_HARDWARE_ID)
    return QString::fromLocal8Bit(AM_HARDWARE_ID);
#elif defined(AM_HARDWARE_ID_FROM_FILE)
    QFile f(QString::fromLocal8Bit(AM_HARDWARE_ID_FROM_FILE));
    if (f.open(QFile::ReadOnly))
        return f.readAll().trimmed();
#else
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces()) {
        if (iface.isValid() && (iface.flags() & QNetworkInterface::IsUp)
                && !(iface.flags() & (QNetworkInterface::IsPointToPoint | QNetworkInterface::IsLoopBack))
                && !iface.hardwareAddress().isEmpty()) {
            return iface.hardwareAddress().replace(qL1C(':'), qL1S("-"));
        }
    }
#endif
    return QString();
}

void am_trace(QDebug)
{ }

void registerUnregisteredDLTContexts()
{
    if (!dltLoggingEnabled)
        return;
#ifdef AM_GENIVIEXTRAS_LAZY_INIT
    globalDltRegistration()->registerUnregisteredContexts();
#endif
}

void changeDLTApplication(const char *dltAppID, const char *dltAppDescription)
{
    if (!dltLoggingEnabled)
        return;
#if defined(QT_GENIVIEXTRAS_LIB)
    globalDltRegistration()->registerApplication(dltAppID, dltAppDescription);
#else
    Q_UNUSED(dltAppID)
    Q_UNUSED(dltAppDescription)
#endif
}

QT_END_NAMESPACE_AM
