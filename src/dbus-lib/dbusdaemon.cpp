/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// include as eary as possible, since the <windows.h> header will re-#define "interface"
#include <QDBusConnection>
#if defined(Q_OS_LINUX)
#  include <sys/prctl.h>
#  include <sys/signal.h>
#endif
#include <QCoreApplication>
#include <QIODevice>

#include "dbusdaemon.h"
#include "utilities.h"
#include "exception.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

DBusDaemonProcess::DBusDaemonProcess(QObject *parent)
    : QProcess(parent)
{
    QString program = qSL("dbus-daemon");
    QStringList arguments = { qSL("--nofork"), qSL("--print-address"), qSL("--session") };

#if defined(Q_OS_MACOS)
    // there's no native dbus support on macOS, so we try to use brew's dbus support
    program = qSL("/usr/local/bin/dbus-daemon");
    // brew's dbus-daemon needs an address, because it will otherwise assume that it was
    // started via launchd and expects its address in $DBUS_LAUNCHD_SESSION_BUS_SOCKET
    QString address = qSL("--address=unix:path=") + QDir::tempPath() + qSL("am-")
            + QString::number(QCoreApplication::applicationPid()) + qSL("-session.bus");

    arguments << address;
#elif defined(Q_OS_WIN)
    arguments << qSL(" --address=tcp:host=localhost");
#endif
    setProgram(program);
    setArguments(arguments);

#if defined(Q_OS_LINUX)
    setChildProcessModifier([]() {
        // at least on Linux we can make sure that those dbus-daemons are always killed
        prctl(PR_SET_PDEATHSIG, SIGKILL);
    });
#endif
}

DBusDaemonProcess::~DBusDaemonProcess()
{
    kill();
    waitForFinished();
}

void DBusDaemonProcess::start() Q_DECL_NOEXCEPT_EXPR(false)
{
    static const int timeout = 10000 * int(timeoutFactor());

    qunsetenv("DBUS_SESSION_BUS_ADDRESS");

    auto dbusDaemon = new DBusDaemonProcess(qApp);
    dbusDaemon->QProcess::start(QIODevice::ReadOnly);
    if (!dbusDaemon->waitForStarted(timeout) || !dbusDaemon->waitForReadyRead(timeout)) {
        throw Exception("could not start a dbus-daemon process (%1): %2")
                .arg(dbusDaemon->program(), dbusDaemon->errorString());
    }
    QByteArray busAddress = dbusDaemon->readAllStandardOutput().trimmed();

    qputenv("DBUS_SESSION_BUS_ADDRESS", busAddress);
    qCInfo(LogSystem, "NOTICE: running on private D-Bus session bus to avoid conflicts:");
    qCInfo(LogSystem, "        DBUS_SESSION_BUS_ADDRESS=%s", busAddress.constData());
}

QT_END_NAMESPACE_AM
