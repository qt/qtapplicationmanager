// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUSDAEMON_H
#define DBUSDAEMON_H

#include <QtAppManCommon/global.h>
#include <QtCore/QProcess>

QT_BEGIN_NAMESPACE_AM

class DBusDaemonProcess : public QProcess // clazy:exclude=missing-qobject-macro
{
public:
    DBusDaemonProcess(QObject *parent = nullptr);
    ~DBusDaemonProcess() override;

    static void start() noexcept(false);
};

QT_END_NAMESPACE_AM

#endif // DBUSDAEMON_H
