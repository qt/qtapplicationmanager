// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

// include as eary as possible, since the <windows.h> header will re-#define "interface"
#include <QDBusConnection>
#if defined(Q_OS_LINUX)
#  include <sys/prctl.h>
#  include <sys/signal.h>
#  include <dlfcn.h>
#  include <QVersionNumber>
#endif
#include <QCoreApplication>
#include <QIODevice>

#include "dbusdaemon.h"
#include "utilities.h"
#include "exception.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

#if defined(Q_OS_LINUX)
static QVersionNumber dbusVersion()
{
    typedef void (*am_dbus_get_version_t)(int *, int *, int *);
    static am_dbus_get_version_t am_dbus_get_version = nullptr;

    if (!am_dbus_get_version)
        am_dbus_get_version = reinterpret_cast<am_dbus_get_version_t>(dlsym(RTLD_DEFAULT, "dbus_get_version"));

    if (!am_dbus_get_version)
        qFatal("ERROR: could not resolve 'dbus_get_version' from libdbus-1");

    int major = 0, minor = 0, patch = 0;
    am_dbus_get_version(&major, &minor, &patch);
    return QVersionNumber(major, minor, patch);
}
#endif

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
    arguments << qSL("--address=tcp:host=localhost");
#elif defined(Q_OS_LINUX)
    // some dbus implementations create an abstract socket by default, while others create
    // a file based one. we need a file based one however, because that socket might get
    // mapped into a container.
    if (dbusVersion() >= QVersionNumber(1, 11, 14)) {
        arguments << qSL("--address=unix:dir=/tmp");
    } else {
        arguments << QString(qSL("--address=unix:path=") + QDir::tempPath() + qSL("am-")
                             + QString::number(QCoreApplication::applicationPid()) + qSL("-session.bus"));
    }
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
