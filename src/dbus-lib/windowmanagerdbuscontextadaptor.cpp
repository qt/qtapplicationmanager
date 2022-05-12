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

#include "windowmanagerdbuscontextadaptor.h"
#include "windowmanager.h"
#include "windowmanager_adaptor.h"

QT_BEGIN_NAMESPACE_AM

WindowManagerDBusContextAdaptor::WindowManagerDBusContextAdaptor(WindowManager *am)
    : AbstractDBusContextAdaptor(am)
{
    m_adaptor = new WindowManagerAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

WindowManagerAdaptor::WindowManagerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{ }

WindowManagerAdaptor::~WindowManagerAdaptor()
{ }

bool WindowManagerAdaptor::runningOnDesktop() const
{
    return WindowManager::instance()->isRunningOnDesktop();
}

bool WindowManagerAdaptor::slowAnimations() const
{
    return WindowManager::instance()->slowAnimations();
}

void WindowManagerAdaptor::setSlowAnimations(bool slow)
{
    WindowManager::instance()->setSlowAnimations(slow);
}

bool WindowManagerAdaptor::makeScreenshot(const QString &filename, const QString &selector)
{
    return WindowManager::instance()->makeScreenshot(filename, selector);
}
