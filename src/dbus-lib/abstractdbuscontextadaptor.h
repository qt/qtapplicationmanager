// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

QT_BEGIN_NAMESPACE_AM

class AbstractDBusContextAdaptor : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    QDBusAbstractAdaptor *generatedAdaptor();
    static QDBusContext *dbusContextFor(QDBusAbstractAdaptor *adaptor);

protected:
    explicit AbstractDBusContextAdaptor(QObject *realObject);
    QDBusAbstractAdaptor *m_adaptor = nullptr;
};

#define AM_AUTHENTICATE_DBUS(RETURN_TYPE) \
    do { \
        if (!DBusPolicy::check(this, __FUNCTION__)) \
            return RETURN_TYPE(); \
    } while (false);


QT_END_NAMESPACE_AM
