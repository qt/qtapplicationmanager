// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QQmlEngine>
#include <QDebug>
#include <QPointer>
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>

#include "global.h"
#include "applicationmain.h"
#include "applicationinterface.h"
#include "dbusapplicationinterfaceimpl.h"
#include "notification.h"
#include "utilities.h"

#ifdef interface // in case windows.h got included somehow
#  undef interface
#endif

QT_BEGIN_NAMESPACE_AM

DBusApplicationInterfaceImpl::DBusApplicationInterfaceImpl(ApplicationMain *applicationMain)
    : ApplicationInterfaceImpl()
    , m_applicationMain(applicationMain)
{
    QObject::connect(applicationMain, &ApplicationMain::quit,
                     applicationMain, [this]() { handleQuit(); });
}

void DBusApplicationInterfaceImpl::attach(ApplicationInterface *iface)
{
    ApplicationInterfaceImpl::attach(iface);

    QObject::connect(m_applicationMain, &ApplicationMain::memoryLowWarning,
                     iface, &ApplicationInterface::memoryLowWarning);
    QObject::connect(m_applicationMain, &ApplicationMain::memoryCriticalWarning,
                     iface, &ApplicationInterface::memoryCriticalWarning);
    QObject::connect(m_applicationMain, &ApplicationMain::openDocument,
                     iface, &ApplicationInterface::openDocument);
}

QString DBusApplicationInterfaceImpl::applicationId() const
{
    return m_applicationMain->applicationId();
}

QVariantMap DBusApplicationInterfaceImpl::name() const
{
    return m_applicationMain->applicationName();
}

QUrl DBusApplicationInterfaceImpl::icon() const
{
    return m_applicationMain->applicationIcon();
}

QString DBusApplicationInterfaceImpl::version() const
{
    return m_applicationMain->applicationVersion();
}

void DBusApplicationInterfaceImpl::acknowledgeQuit()
{
    QCoreApplication::instance()->quit();
}

QVariantMap DBusApplicationInterfaceImpl::systemProperties() const
{
    return m_applicationMain->systemProperties();
}

QVariantMap DBusApplicationInterfaceImpl::applicationProperties() const
{
    return m_applicationMain->applicationProperties();
}

QT_END_NAMESPACE_AM
