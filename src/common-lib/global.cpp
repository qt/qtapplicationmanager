/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
QDLT_REGISTER_APPLICATION("PCAM", "Pelagicore Application-Manager")
QDLT_FALLBACK_CATEGORY(LogSystem)
#else
#  define QDLT_LOGGING_CATEGORY(a,b,c,d) Q_LOGGING_CATEGORY(a,b)
#endif

QDLT_LOGGING_CATEGORY(LogSystem, "am.system", "SYS", "General messages")
QDLT_LOGGING_CATEGORY(LogInstaller, "am.installer", "INST", "Installer sub-system")
QDLT_LOGGING_CATEGORY(LogWayland, "am.wayland", "WAYL", "Wayland sub-system")
QDLT_LOGGING_CATEGORY(LogQml, "am.qml", "QML", "QML messages")
QDLT_LOGGING_CATEGORY(LogNotifications, "am.notify", "NTFY", "Notification sub-system")
QDLT_LOGGING_CATEGORY(LogQmlRuntime, "am.runtime.qml", "QMRT", "QML runtime")
QDLT_LOGGING_CATEGORY(LogQmlIpc, "am.qml.ipc", "QMIP", "QML IPC")


void colorLogToStderr(QtMsgType msgType, const QMessageLogContext &context, const QString &message)
{
#if defined(QT_GENIVI_EXTRAS_LIB)
    QDltRegistration::messageHandler(msgType, context, message);
#endif

    QString file = QFileInfo(QFile::decodeName(context.file)).fileName();

    enum ConsoleColor { Off, Black, Red, Green, Yellow, Blue, Magenta, Cyan, Gray, BrightIndex, Bright = 0x80 };
    QMap<int, int> colors; // %<n> -> ConsoleColor
    const char *msgTypeStr[] = { "DBG ", "WARN", "CRIT", "FATL", "INFO" };
    ConsoleColor msgTypeColor[] = { Green, Yellow, Red, Magenta, Blue };

    if (msgType < QtDebugMsg || msgType > QtInfoMsg)
        msgType = QtCriticalMsg;

    QByteArray fmt("[%1 | %2] %3 %6[%4:%5]\n");

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
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h, &csbi))
            windowWidth = csbi.dwSize.X;
    }

    static const WORD winColors[] = {
        FOREGROUND_INTENSITY,               // off
        0,                                  // black
        FOREGROUND_RED,                     // red
        FOREGROUND_GREEN,                   // green
        FOREGROUND_GREEN | FOREGROUND_RED,  // yellow
        FOREGROUND_BLUE,                    // blue
        FOREGROUND_BLUE | FOREGROUND_RED,   // magenta
        FOREGROUND_BLUE | FOREGROUND_GREEN, // cyan
        FOREGROUND_INTENSITY,               // gray
        FOREGROUND_INTENSITY                // bright
    };
#else
    if (::isatty(STDERR_FILENO)) {
        struct ::winsize ws;
        if (::ioctl(0, TIOCGWINSZ, &ws) == 0)
            windowWidth = ws.ws_col;
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
    if (lastlineLength < windowWidth)
        spacing = windowWidth - lastlineLength - 1;
    if (lastlineLength > windowWidth)
        spacing = ((lastlineLength / windowWidth) + 1) * windowWidth - lastlineLength - 1;

    args.last() = QString(spacing, QLatin1Char(' ')); // right align spacing

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
                    fputs(out.constData(), stderr);
                    out.clear();
#if defined(Q_OS_WIN)
                    SetConsoleTextAttribute(h, (color & Bright ? winColors[BrightIndex] : 0) | winColors[color & ~Bright]);
#else
                    if (color & Bright)
                        fputs(ansiColors[BrightIndex], stderr);
                    fputs(ansiColors[color & ~Bright], stderr);
#endif
                    fputs(args.at(c2 - '0' - 1).toLocal8Bit().constData(), stderr);
#if defined(Q_OS_WIN)
                    SetConsoleTextAttribute(h, winColors[Off]);
#else
                    fputs(ansiColors[Off], stderr);
#endif
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
