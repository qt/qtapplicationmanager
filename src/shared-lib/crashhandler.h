// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QtAppManShared/qtappmansharedglobal.h>
#include <QtCore/QVariantMap>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)

QT_BEGIN_NAMESPACE_AM

namespace CrashHandler {

Q_APPMANSHARED_EXPORT void setCrashActionConfiguration(bool printBacktrace, bool printQmlStack,
                                                       int waitForGdbAttach, bool dumpCore,
                                                       bool dumpCoreOnWatchdogKill,
                                                       int stackFramesToIgnoreOnCrash,
                                                       int stackFramesToIgnoreOnException);
Q_APPMANSHARED_EXPORT void setQmlEngine(QQmlEngine *engine);

}

QT_END_NAMESPACE_AM

#endif // CRASHHANDLER_H
