// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QMetaMethod>
#include "applicationinterface.h"
#include "applicationinterfaceimpl.h"

QT_BEGIN_NAMESPACE_AM

std::function<ApplicationInterfaceImpl *(ApplicationInterface *)> ApplicationInterfaceImpl::s_factory;

void ApplicationInterfaceImpl::setFactory(const std::function<ApplicationInterfaceImpl *(ApplicationInterface *)> &factory)
{
    s_factory = factory;
}

ApplicationInterfaceImpl::ApplicationInterfaceImpl()
{ }

ApplicationInterfaceImpl *ApplicationInterfaceImpl::create(ApplicationInterface *iface)
{
    auto aminterface = s_factory ? s_factory(iface) : nullptr;
    if (aminterface)
        aminterface->attach(iface);
    return aminterface;
}

void ApplicationInterfaceImpl::attach(ApplicationInterface *iface)
{
    m_aminterfaces << iface;
}

void ApplicationInterfaceImpl::detach(ApplicationInterface *iface)
{
    m_aminterfaces.removeAll(iface);
}

QList<ApplicationInterface *> ApplicationInterfaceImpl::amInterfaces()
{
    return m_aminterfaces;
}

void ApplicationInterfaceImpl::handleQuit()
{
    static const QMetaMethod quitSignal = QMetaMethod::fromSignal(&ApplicationInterface::quit);

    bool isQuitConnected = false;
    for (auto aminterface : std::as_const(m_aminterfaces)) {
        if (aminterface->isSignalConnected(quitSignal)) {
            emit aminterface->quit();
            isQuitConnected = true;
        }
    }
    if (!isQuitConnected)
        acknowledgeQuit();
}

QT_END_NAMESPACE_AM

#include "moc_applicationinterfaceimpl.cpp"
