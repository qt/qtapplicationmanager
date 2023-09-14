// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "dbuscontextadaptor.h"


QT_BEGIN_NAMESPACE_AM

bool DBusContextAdaptor::registerOnDBus(QDBusConnection conn, const QString &path)
{
    return m_isRegistered = conn.registerObject(path, this, QDBusConnection::ExportAdaptors);
}

bool DBusContextAdaptor::isRegistered() const
{
    return m_isRegistered;
}

DBusContextAdaptor::DBusContextAdaptor(QObject *realObject)
    : QObject(realObject)
{ }

QDBusContext *QtAM::DBusContextAdaptor::dbusContextFor(QDBusAbstractAdaptor *adaptor)
{
    return adaptor ? qobject_cast<DBusContextAdaptor *>(adaptor->parent()) : nullptr;
}

QT_END_NAMESPACE_AM

#include "moc_dbuscontextadaptor.cpp"
