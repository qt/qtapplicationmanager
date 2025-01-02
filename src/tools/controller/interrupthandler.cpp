// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QThread>
#include <QCoreApplication>
#include "interrupthandler.h"
#include "unixsignalhandler.h"

#if defined(Q_OS_UNIX)
#  include <sys/poll.h>
#  define AM_SIGNALS  { SIGTERM, SIGINT, SIGPIPE, SIGHUP }
#else
#  define AM_SIGNALS  { SIGTERM, SIGINT }
#endif


QT_BEGIN_NAMESPACE_AM

#if defined(POLLRDHUP)
// ssh does not forward Ctrl+C, but we can detect a hangup condition on stdin
class HupThread : public QThread // clazy:exclude=missing-qobject-macro
{
public:
    HupThread(QCoreApplication *parent)
        : QThread(parent)
    {
        connect(parent, &QCoreApplication::aboutToQuit, this, [this]() {
            if (isRunning()) {
                terminate();
                wait();
            }
        });
    }

    void run() override
    {
        while (true) {
            struct pollfd pfd = { 0, POLLRDHUP, 0 };
            int res = poll(&pfd, 1, -1);
            if (res == 1 && pfd.revents & POLLHUP) {
                kill(getpid(), SIGHUP);
                return;
            }
        }
    }
};
#endif // defined(POLLRDHUP)


void InterruptHandler::install(const std::function<void (int)> &handler)
{
    static bool once = false;

    if (!once) {
#if defined(POLLRDHUP)
        (new HupThread(qApp))->start();
#endif
        once = true;
    }

    // on Ctrl+C or SIGTERM -> stop the application
    UnixSignalHandler::instance()->resetToDefault(AM_SIGNALS);
    UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler,
                                           AM_SIGNALS, handler);
}

QT_END_NAMESPACE_AM
