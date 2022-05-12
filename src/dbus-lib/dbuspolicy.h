/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QtAppManCommon/global.h>
#include <QVariantMap>
#include <QByteArray>

QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

QT_BEGIN_NAMESPACE_AM

class DBusPolicy
{
public:
    static bool add(QDBusAbstractAdaptor *dbusAdaptor, const QVariantMap &yamlFragment);
    static bool check(QDBusAbstractAdaptor *dbusAdaptor, const QByteArray &function);
};

QT_END_NAMESPACE_AM

