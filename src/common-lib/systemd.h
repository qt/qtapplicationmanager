// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSTEMD_H
#define SYSTEMD_H

#include <chrono>
#include <optional>

#include <QtCore/QByteArray>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Systemd
{
public:
    static Systemd *instance();
    ~Systemd();

    bool notify(const QString &state);

    std::optional<std::chrono::milliseconds> watchdogTimeout(bool ignorePid = false);

private:
    Systemd();
    Systemd(const Systemd &) = delete;
    Systemd(Systemd &&) = delete;
    Systemd &operator=(const Systemd &) = delete;
    Systemd &operator=(Systemd &&) = delete;

    bool checkPid(const QByteArray &pidVar);

    QByteArray m_notifySocket;
    QByteArray m_watchdogUsec;
    QByteArray m_watchdogPid;
    int m_notifySocketFd = -1;
    bool m_notifySocketTriedToConnect = false;
};

QT_END_NAMESPACE_AM

#endif // SYSTEMD_H
