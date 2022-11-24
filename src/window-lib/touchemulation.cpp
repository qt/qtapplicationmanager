/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtAppManCommon/global.h>
#include <QGuiApplication>
#include <QInputDevice>
#include "touchemulation.h"
#if defined(AM_ENABLE_TOUCH_EMULATION)
#  include "touchemulation_x11_p.h"
#endif

QT_BEGIN_NAMESPACE_AM

TouchEmulation *TouchEmulation::s_instance = nullptr;

TouchEmulation::~TouchEmulation()
{
    s_instance = nullptr;
}

TouchEmulation *TouchEmulation::instance()
{
    return s_instance;
}

TouchEmulation *TouchEmulation::createInstance()
{
    if (Q_UNLIKELY(s_instance))
        qFatal("TouchEmulation::createInstance() was called a second time.");

#if defined(AM_ENABLE_TOUCH_EMULATION)
    if (isSupported())
        s_instance = new TouchEmulationX11();
#endif

    return s_instance;
}

bool TouchEmulation::isSupported()
{
#if defined(AM_ENABLE_TOUCH_EMULATION)
    if (QGuiApplication::platformName() == qL1S("xcb"))
        return true;
#endif
    return false;
}

bool QtAM::TouchEmulation::hasPhysicalTouchscreen()
{
    const auto devs = QInputDevice::devices();
    for (auto dev : devs) {
        if (dev->type() == QInputDevice::DeviceType::TouchScreen)
            return true;
    }
    return false;
}

QT_END_NAMESPACE_AM

#include "moc_touchemulation.cpp"
