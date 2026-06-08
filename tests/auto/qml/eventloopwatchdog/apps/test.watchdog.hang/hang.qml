// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// The window is created first, then a timer handler blocks the main thread forever. The
// event-loop watchdog has to detect the stuck thread and kill it.
ApplicationManagerWindow {
    Timer {
        interval: 100
        running: true
        onTriggered: {
            while (true) ; // block the event loop forever
        }
    }
}
