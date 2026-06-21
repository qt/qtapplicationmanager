// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <private/qabstractanimation_p.h>
#include "slowanimhelper.h"

SlowAnimHelper::SlowAnimHelper(QObject *parent)
    : QObject(parent)
{}

qreal SlowAnimHelper::speedModifier() const
{
    // mirror how the runtime applies slow animations (see launcher-qml.cpp / windowmanager.cpp):
    // before 6.11 there was no speed modifier, only a slow-mode flag, but that is private.
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    return qreal(-1);
#else
    return QUnifiedTimer::instance()->getSpeedModifier();
#endif
}
