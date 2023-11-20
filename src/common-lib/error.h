// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QDebug>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

enum class Error {
    None = 0,
    Canceled = 1,
    Parse = 2,

    System = 10,
    IO = 11,
    Permissions = 12,
    Network = 13,
    StorageSpace = 14,
    DBus = 15,

    Cryptography = 20,

    Archive = 30,

    Package = 40,
    Locked = 41,
    NotInstalled = 42,
    AlreadyInstalled = 43,

    MediumNotAvailable = 50,
    WrongMedium = 51,

    Intents = 60
};

inline QDebug &operator<<(QDebug &debug, Error error)
{
    return debug << int(error);
}

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QtAM::Error)
