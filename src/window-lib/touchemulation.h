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
#include <QObject>

QT_BEGIN_NAMESPACE_AM

/*
   Converts native mouse events into touch events

   Useful for emulating touch interaction using a mouse device since touch input
   follows a completely different code path from mouse events in Qt.
*/
class TouchEmulation : public QObject
{
    Q_OBJECT

public:
    virtual ~TouchEmulation();
    static TouchEmulation *createInstance();
    static TouchEmulation *instance();

    static bool isSupported();
    static bool hasPhysicalTouchscreen();

private:
    static TouchEmulation *s_instance;
};

QT_END_NAMESPACE_AM
