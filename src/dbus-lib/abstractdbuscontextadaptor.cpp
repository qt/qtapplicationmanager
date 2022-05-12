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
