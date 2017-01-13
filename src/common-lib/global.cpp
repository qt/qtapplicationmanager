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
#include <QFileInfo>
#include <QNetworkInterface>

#include "global.h"
#include "utilities.h"

#if defined(Q_OS_WIN)
Q_CORE_EXPORT void qWinMsgHandler(QtMsgType t, const char* str);
#  include <windows.h>
#endif
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <termios.h>
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

    QString file = QFileInfo(QFile::decodeName(context.file)).fileName();

    enum ConsoleColor { Off, Black, Red, Green, Yellow, Blue, Magenta, Cyan, Gray, BrightIndex, Bright = 0x80 };
    QMap<int, int> colors; // %<n> -> ConsoleColor
    const char *msgTypeStr[] = { "DBG ", "WARN", "CRIT", "FATL", "INFO" };
    ConsoleColor msgTypeColor[] = { Green, Yellow, Red, Magenta, Blue };

    if (msgType < QtDebugMsg || msgType > QtInfoMsg)
        msgType = QtCriticalMsg;

    QByteArray fmt = "[%1 | %2] %3 %6[%4:%5]\n";

    QStringList args = QStringList()
        << qL1S(msgTypeStr[msgType])
        << qL1S(context.category)
        << message
        << file
        << QString::number(context.line)
        << QString();

    colors[1] = Bright | msgTypeColor[msgType];
    colors[2] = (Red + qHash(QByteArray(context.category)) % 7);
    colors[4] = Magenta;
    colors[5] = Magenta | Bright;

    if (!colorLogApplicationId.isEmpty()) {
        fmt = "[%1 | %2 | %7] %3 %6[%4:%5]\n";
        args << QString::fromLocal8Bit(colorLogApplicationId);
        colors[7] = (Red + qHash(colorLogApplicationId) % 7);
    }

    QString str = QString::fromLatin1(fmt);
    for (int i = 0; i < args.size(); ++i)
        str = str.arg(args.at(i));

    int lastlineLength = str.size() - 1;
    int nlpos = str.lastIndexOf(QLatin1Char('\n'), str.size() - 2);
    if (nlpos >= 0)
        lastlineLength = str.size() - nlpos - 2;
    int spacing = 0;
    int windowWidth = -1;

#if defined(Q_OS_WIN)
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        str.append(QLatin1Char('\n'));

        // do not use QMutex to avoid possible recursions
        static CRITICAL_SECTION cs;
        static bool csInitialized = false;
        if (!csInitialized)
            InitializeCriticalSection(&cs);

        EnterCriticalSection(&cs);
        OutputDebugStringW(reinterpret_cast<const wchar_t *>(str.utf16()));
        LeaveCriticalSection(&cs);
        return;
    } else {
        if (canOutputAnsiColors(2)) {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(h, &csbi))
                windowWidth = csbi.dwSize.X;
        }
    }
#else
    static bool useAnsiColors = canOutputAnsiColors(STDERR_FILENO);

    if (useAnsiColors) {
        windowWidth = 120;
        if (::isatty(STDERR_FILENO)) {
            struct ::winsize ws;
            if ((::ioctl(0, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
                windowWidth = ws.ws_col;
        }
    }

#endif
    if (windowWidth <= 0) {
#if defined(Q_OS_ANDROID)
        android_LogPriority pri = ANDROID_LOG_DEBUG;
        switch (msgType) {
        default:
        case QtDebugMsg:    pri = ANDROID_LOG_DEBUG; break;
        case QtWarningMsg:  pri = ANDROID_LOG_WARN; break;
        case QtCriticalMsg: pri = ANDROID_LOG_ERROR; break;
        case QtFatalMsg:    pri = ANDROID_LOG_FATAL; break;
        };

        static QByteArray appName = QCoreApplication::applicationName().toLocal8Bit();

        __android_log_print(pri, appName.constData(), "%s\n", str.toLocal8Bit().constData());
#else
        fputs(str.toLocal8Bit().constData(), stderr);
        fflush(stderr);
#endif
        return;
    }
    static const char *ansiColors[] = {
        "\x1b[0m",  // off
        "\x1b[30m", // black
        "\x1b[31m", // red
        "\x1b[32m", // green
        "\x1b[33m", // yellow
        "\x1b[34m", // blue
        "\x1b[35m", // magenta
        "\x1b[36m", // cyan
        "\x1b[37m", // gray
        "\x1b[1m"   // bright
    };

    if (lastlineLength < windowWidth)
        spacing = windowWidth - lastlineLength - 1;
    if (lastlineLength > windowWidth)
        spacing = ((lastlineLength / windowWidth) + 1) * windowWidth - lastlineLength - 1;

    args[5] = QString(spacing, QLatin1Char(' ')); // right align spacing

    QByteArray out;
    for (int i = 0; i < fmt.size(); ++i) {
        char c = fmt[i];
        if (c == '%' && i < (fmt.size() - 1)) {
            char c2 = fmt[++i];
            if (c2 == '%') {
                out += '%';
            } else if (c2 >= '1' && c2 <= '9') {
                if (colors.contains(c2 - '0')) {
                    int color = colors.value(c2 - '0');
                    if (color & Bright)
                        out += ansiColors[BrightIndex];
                    out += ansiColors[color & ~Bright];
                    out += args.at(c2 - '0' - 1).toLocal8Bit();
                    out += ansiColors[Off];
                    continue;
                } else {
                    out += args.at(c2 - '0' - 1).toLocal8Bit();
                }
            }
        } else {
            out += c;
        }
    }
    fputs(out.constData(), stderr);
    fflush(stderr);
    return;
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
