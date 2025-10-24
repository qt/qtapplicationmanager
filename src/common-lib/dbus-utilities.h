// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUS_UTILITIES_H
#define DBUS_UTILITIES_H

#include <QtAppManCommon/qtappmancommonglobal.h>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE_AM

Q_APPMANCOMMON_EXPORT QVariant convertToDBusVariant(const QVariant &variant);

Q_APPMANCOMMON_EXPORT QVariant convertFromDBusVariant(const QVariant &variant);

Q_APPMANCOMMON_EXPORT void registerDBusTypes();

Q_APPMANCOMMON_EXPORT void ensureLibDBusIsAvailable();

QT_END_NAMESPACE_AM

#endif // DBUS_UTILITIES_H
