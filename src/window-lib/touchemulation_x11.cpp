/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <QtAppManCommon/global.h>

#include "touchemulation_x11_p.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QTouchDevice>
#include <QWindow>
#include <QTest>

#include <QtAppManCommon/logging.h>

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>

#include <xcb/xcb.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>

// this event type was added in libxcb 1.10,
// but we support also older version
#ifndef XCB_GE_GENERIC
#  define XCB_GE_GENERIC 35
#endif

QT_BEGIN_NAMESPACE_AM

using QTest::QTouchEventSequence;


static Qt::MouseButton xcbButtonToQtMouseButton(xcb_button_t detail)
{
    switch (detail) {
    case 1: return Qt::LeftButton;
    case 2: return Qt::MidButton;
    case 3: return Qt::RightButton;
        // don't care about the rest
    default: return Qt::NoButton;
    }
}

// Function copied from the XCB QPA
static void xi2PrepareXIGenericDeviceEvent(xcb_ge_event_t *event)
{
    // xcb event structs contain stuff that wasn't on the wire, the full_sequence field
    // adds an extra 4 bytes and generic events cookie data is on the wire right after the standard 32 bytes.
    // Move this data back to have the same layout in memory as it was on the wire
    // and allow casting, overwriting the full_sequence field.
    memmove(reinterpret_cast<char *>(event) + 32, reinterpret_cast<char*>(event) + 36, event->length * 4);
}

static qreal fixed1616ToReal(FP1616 val)
{
    return qreal(val) / 0x10000;
}


TouchEmulationX11::TouchEmulationX11()
{
    qGuiApp->installNativeEventFilter(this);

    // Create a fake touch device to deliver our synthesized events
    m_touchDevice = new QTouchDevice;
    m_touchDevice->setType(QTouchDevice::TouchScreen);
    QWindowSystemInterface::registerTouchDevice(m_touchDevice);

    queryForXInput2();
}

void TouchEmulationX11::queryForXInput2()
{
    QPlatformNativeInterface *nativeInterface = qGuiApp->platformNativeInterface();
    Display *xDisplay = static_cast<Display*>(nativeInterface->nativeResourceForIntegration("Display"));

    int xiOpCode, xiEventBase, xiErrorBase;
    if (xDisplay && XQueryExtension(xDisplay, "XInputExtension", &xiOpCode, &xiEventBase, &xiErrorBase)) {
        // 2.0 is enough for our needs
        int xiMajor = 2;
        int xi2Minor = 0;
        m_haveXInput2 = XIQueryVersion(xDisplay, &xiMajor, &xi2Minor) != BadRequest;
    }
}

bool TouchEmulationX11::nativeEventFilter(const QByteArray &eventType, void *message, long * /*result*/)
{
    if (eventType != "xcb_generic_event_t")
        return false; // just ignore non-XCB-native events

    xcb_generic_event_t *xcbEvent = static_cast<xcb_generic_event_t *>(message);

    switch (xcbEvent->response_type & ~0x80) {
    case XCB_BUTTON_PRESS: {
        auto xcbPress = reinterpret_cast<xcb_button_press_event_t *>(xcbEvent);
        return handleButtonPress(static_cast<WId>(xcbPress->event), xcbPress->detail, 0,
                                 xcbPress->event_x, xcbPress->event_y);
    }
    case XCB_BUTTON_RELEASE: {
        auto xcbRelease = reinterpret_cast<xcb_button_release_event_t *>(xcbEvent);
        return handleButtonRelease(static_cast<WId>(xcbRelease->event), xcbRelease->detail, 0,
                                   xcbRelease->event_x, xcbRelease->event_y);
    }
    case XCB_MOTION_NOTIFY: {
        auto xcbMotion = reinterpret_cast<xcb_motion_notify_event_t *>(xcbEvent);
        return handleMotionNotify(static_cast<WId>(xcbMotion->event), 0,
                                  xcbMotion->event_x, xcbMotion->event_y);
    }
    case XCB_GE_GENERIC: {
        if (!m_haveXInput2)
            return false;

        auto xcbGeneric = reinterpret_cast<xcb_ge_event_t *>(xcbEvent);

        // Because xi2PrepareXIGenericDeviceEvent modifies data inplace
        backupEventData(xcbGeneric);

        xi2PrepareXIGenericDeviceEvent(xcbGeneric);
        auto xiEvent = reinterpret_cast<xXIGenericDeviceEvent *>(xcbGeneric);
        xXIDeviceEvent *xiDeviceEvent = nullptr;

        switch (xiEvent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_Motion:
            xiDeviceEvent = reinterpret_cast<xXIDeviceEvent *>(xcbGeneric);
            break;
        default:
            break;
        }

        bool result = false;

        if (xiDeviceEvent) {
            switch (xiDeviceEvent->evtype) {
            case XI_ButtonPress:
                result = handleButtonPress(
                            static_cast<WId>(xiDeviceEvent->event),
                            xiDeviceEvent->detail,
                            xiDeviceEvent->mods.base_mods,
                            fixed1616ToReal(xiDeviceEvent->event_x),
                            fixed1616ToReal(xiDeviceEvent->event_y));
                break;
            case XI_ButtonRelease:
                result = handleButtonRelease(
                            static_cast<WId>(xiDeviceEvent->event),
                            xiDeviceEvent->detail,
                            xiDeviceEvent->mods.base_mods,
                            fixed1616ToReal(xiDeviceEvent->event_x),
                            fixed1616ToReal(xiDeviceEvent->event_y));
                break;
            case XI_Motion:
                result = handleMotionNotify(
                            static_cast<WId>(xiDeviceEvent->event),
                            xiDeviceEvent->mods.base_mods,
                            fixed1616ToReal(xiDeviceEvent->event_x),
                            fixed1616ToReal(xiDeviceEvent->event_y));
                result = true;
                break;
            default:
                break;
            }
        }

        // Put the event back in its original state so that the XCB QPA can process it normally
        restoreEventData(xcbGeneric);
        return result;
    }
    default:
        return false;
    };
}

bool TouchEmulationX11::handleButtonPress(WId windowId, uint32_t detail, uint32_t /*modifiers*/, qreal x, qreal y)
{
    Qt::MouseButton button = xcbButtonToQtMouseButton(static_cast<xcb_button_t>(detail));

    // Filter out the other mouse buttons
    if (button != Qt::LeftButton)
        return true;

    QWindow *targetWindow = findQWindowWithXWindowID(windowId);

    QPointF windowPos(x / targetWindow->devicePixelRatio(), y / targetWindow->devicePixelRatio());

    QTouchEventSequence touchEvent = QTest::touchEvent(targetWindow, m_touchDevice, false /* autoCommit */);
    touchEvent.press(0 /* touchId */, windowPos.toPoint(), targetWindow);
    touchEvent.commit(false /* processEvents */);

    m_leftButtonIsPressed = true;
    return true;
}

bool TouchEmulationX11::handleButtonRelease(WId windowId, uint32_t detail, uint32_t, qreal x, qreal y)
{
    Qt::MouseButton button = xcbButtonToQtMouseButton(static_cast<xcb_button_t>(detail));

    // Don't eat the event if it wasn't a left mouse press
    if (button != Qt::LeftButton)
        return false;

    QWindow *targetWindow = findQWindowWithXWindowID(windowId);

    QPointF windowPos(x / targetWindow->devicePixelRatio(), y / targetWindow->devicePixelRatio());

    QTouchEventSequence touchEvent = QTest::touchEvent(targetWindow, m_touchDevice, false /* autoCommit */);
    touchEvent.release(0 /* touchId */, windowPos.toPoint(), targetWindow);
    touchEvent.commit(false /* processEvents */);

    m_leftButtonIsPressed = false;
    return true;
}

bool TouchEmulationX11::handleMotionNotify(WId windowId, uint32_t /*modifiers*/, qreal x, qreal y)
{
    if (!m_leftButtonIsPressed)
        return true;

    QWindow *targetWindow = findQWindowWithXWindowID(windowId);

    QPointF windowPos(x / targetWindow->devicePixelRatio(), y / targetWindow->devicePixelRatio());

    QTouchEventSequence touchEvent = QTest::touchEvent(targetWindow, m_touchDevice, false /* autoCommit */);
    touchEvent.move(0 /* touchId */, windowPos.toPoint(), targetWindow);
    touchEvent.commit(false /* processEvents */);

    return true;
}

QWindow *TouchEmulationX11::findQWindowWithXWindowID(WId windowId)
{
    QWindowList windowList = QGuiApplication::topLevelWindows();
    QWindow *foundWindow = nullptr;

    int i = 0;
    while (!foundWindow && (i < windowList.count())) {
        QWindow *window = windowList[i];
        if (window->winId() == windowId)
            foundWindow = window;
        else
            ++i;
    }

    Q_ASSERT(foundWindow);
    return foundWindow;
}


// backup event data before a xi2PrepareXIGenericDeviceEvent() call
void TouchEmulationX11::backupEventData(void *event)
{
    memcpy(reinterpret_cast<char *>(m_xiEventBackupData), reinterpret_cast<char *>(event) + 32, 4);
}

// restore event data after a xi2PrepareXIGenericDeviceEvent() call
void TouchEmulationX11::restoreEventData(void *ev)
{
    auto *event = static_cast<xcb_ge_event_t *>(ev);

    memmove(reinterpret_cast<char *>(event) + 36, reinterpret_cast<char *>(event) + 32, event->length * 4);
    memcpy(reinterpret_cast<char *>(event) + 32, reinterpret_cast<char *>(m_xiEventBackupData), 4);
}

QT_END_NAMESPACE_AM
