// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDBusAbstractAdaptor>

#include "abstractdbuscontextadaptor.h"

QT_BEGIN_NAMESPACE_AM

AbstractDBusContextAdaptor::AbstractDBusContextAdaptor(QObject *am)
    : QObject(am)
{ }

QDBusAbstractAdaptor *AbstractDBusContextAdaptor::generatedAdaptor()
{
    return m_adaptor;
}

QDBusContext *AbstractDBusContextAdaptor::dbusContextFor(QDBusAbstractAdaptor *adaptor)
{
    return adaptor ? qobject_cast<AbstractDBusContextAdaptor *>(adaptor->parent()) : nullptr;
}

QT_END_NAMESPACE_AM

#include "moc_abstractdbuscontextadaptor.cpp"
