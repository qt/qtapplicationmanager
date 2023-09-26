// Copyright (C) 2033The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QMetaMethod>

#include "applicationinterface.h"
#include "applicationinterfaceimpl.h"

QT_BEGIN_NAMESPACE_AM

ApplicationInterfaceImpl::ApplicationInterfaceImpl(ApplicationInterface *ai)
    : m_interface(ai)
{ }

ApplicationInterface *ApplicationInterfaceImpl::applicationInterface()
{
    return m_interface;
}

void ApplicationInterfaceImpl::handleQuit()
{
    static const QMetaMethod quitSignal = QMetaMethod::fromSignal(&ApplicationInterface::quit);

    Q_ASSERT(m_interface);

    if (m_interface->isSignalConnected(quitSignal))
        emit m_interface->quit();
    else
        m_interface->acknowledgeQuit();
}

QT_END_NAMESPACE_AM
