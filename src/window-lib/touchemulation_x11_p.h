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

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManWindow/qtappman_window-config.h>
#include "touchemulation.h"

#if defined(AM_ENABLE_TOUCH_EMULATION)

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QWindow>

QT_FORWARD_DECLARE_CLASS(QPointingDevice)
QT_FORWARD_DECLARE_CLASS(QWindow)

QT_BEGIN_NAMESPACE_AM

class TouchEmulationX11 : public TouchEmulation, public QAbstractNativeEventFilter
{
public:
    TouchEmulationX11();
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void queryForXInput2();
    bool handleButtonPress(WId windowId, uint32_t detail, uint32_t modifiers, qreal x, qreal y);
    bool handleButtonRelease(WId windowId, uint32_t detail, uint32_t, qreal x, qreal y);
    bool handleMotionNotify(WId windowId, uint32_t modifiers, qreal x, qreal y);
    QWindow *findQWindowWithXWindowID(WId windowId);

    void backupEventData(void *event);
    void restoreEventData(void *event);

    QPointingDevice *m_touchDevice = nullptr;
    bool m_haveXInput2 = false;
    bool m_leftButtonIsPressed = false;

    quint8 m_xiEventBackupData[4];
};

QT_END_NAMESPACE_AM

#endif // AM_ENABLE_TOUCH_EMULATION
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
