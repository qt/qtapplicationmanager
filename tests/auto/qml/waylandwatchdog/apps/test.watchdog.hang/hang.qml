// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// The window is created first (so the compositor starts watching this client), then the
// event loop is blocked. The client can no longer reply to Wayland pings and the watchdog
// has to kill it.
ApplicationManagerWindow {
    Timer {
        interval: 100
        running: true
        onTriggered: {
            while (true) ; // block the event loop forever
        }
    }
}
