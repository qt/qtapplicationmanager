// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>

QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

QT_BEGIN_NAMESPACE_AM

class DBusPolicy
{
public:
    static bool add(const QDBusAbstractAdaptor *dbusAdaptor, const QVariantMap &yamlFragment);
    static bool check(const QDBusAbstractAdaptor *dbusAdaptor, const QByteArray &function);
};

QT_END_NAMESPACE_AM

