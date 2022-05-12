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
#include <QObject>
#include <QDBusContext>

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
