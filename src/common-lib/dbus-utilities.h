// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUS_UTILITIES_H
#define DBUS_UTILITIES_H

#include <QtAppManCommon/global.h>
#include <QtCore/QVariant>

#if defined(QT_DBUS_LIB)
#  include <QtDBus/QDBusUnixFileDescriptor>

QT_BEGIN_NAMESPACE_AM
typedef QMap<QString, QDBusUnixFileDescriptor> UnixFdMap;
QT_END_NAMESPACE_AM
Q_DECLARE_METATYPE(QtAM::UnixFdMap)
#endif


QT_BEGIN_NAMESPACE_AM

QVariant convertFromJSVariant(const QVariant &variant);

QVariant convertFromDBusVariant(const QVariant &variant);

void registerDBusTypes();

void ensureLibDBusIsAvailable();

QT_END_NAMESPACE_AM

#endif // DBUS_UTILITIES_H
