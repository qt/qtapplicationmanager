// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// A continuously rendering client: every frame completes quickly, so the quick-window
// watchdog sees normal Sync/Render/Swap transitions and must never kill it.
ApplicationManagerWindow {
    Rectangle {
        anchors.centerIn: parent
        width: 50; height: 50
        color: "green"
        RotationAnimation on rotation {
            from: 0; to: 360; duration: 1000
            loops: Animation.Infinite
            running: true
        }
    }
}
