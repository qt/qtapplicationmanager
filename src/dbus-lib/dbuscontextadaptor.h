// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusAbstractAdaptor>

QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

QT_BEGIN_NAMESPACE_AM


class DBusContextAdaptor : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    bool registerOnDBus(QDBusConnection conn, const QString &path);
    bool isRegistered() const;

    static QDBusContext *dbusContextFor(QDBusAbstractAdaptor *adaptor);

    template<typename T>
    T *generatedAdaptor() { return qobject_cast<T *>(m_adaptor); }

    template<typename T>
    static DBusContextAdaptor *create(QObject *realObject)
    {
        auto that = new DBusContextAdaptor(realObject);
        that->m_adaptor = new T(that);
        return that;
    }

protected:
    QDBusAbstractAdaptor *m_adaptor = nullptr;
    bool m_isRegistered = false;

private:
    explicit DBusContextAdaptor(QObject *realObject);
};


#define AM_AUTHENTICATE_DBUS(RETURN_TYPE) \
do { \
    if (!DBusPolicy::instance()->check(this, __FUNCTION__)) \
        return RETURN_TYPE(); \
} while (false);


QT_END_NAMESPACE_AM
