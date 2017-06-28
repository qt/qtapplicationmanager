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

#if defined(Q_OS_OSX)
    // there's no native dbus support on macOS, so we try to use brew's dbus support
    program = qSL("/usr/local/bin/dbus-daemon");
    // brew's dbus-daemon needs an address, because it will otherwise assume that it was
    // started via launchd and expects its address in $DBUS_LAUNCHD_SESSION_BUS_SOCKET
    QString address = qSL("--address=unix:path=") + QDir::tempPath() + qSL("am-")
            + QString::number(QCoreApplication::applicationPid()) + qSL("-session.bus");

    arguments << address;
#endif
    setProgram(program);
    setArguments(arguments);
}

DBusDaemonProcess::~DBusDaemonProcess()
{
    kill();
    waitForFinished();
}

void DBusDaemonProcess::setupChildProcess()
{
#  if defined(Q_OS_LINUX)
    // at least on Linux we can make sure that those dbus-daemons are always killed
    prctl(PR_SET_PDEATHSIG, SIGKILL);
#  endif
    QProcess::setupChildProcess();
}

void DBusDaemonProcess::start() Q_DECL_NOEXCEPT_EXPR(false)
{
    static const int timeout = 10000 * timeoutFactor();

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
