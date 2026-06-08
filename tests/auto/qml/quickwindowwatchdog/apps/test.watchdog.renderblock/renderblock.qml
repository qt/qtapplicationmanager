// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// Renders normally for a moment (so the window is being watched and is mid-frame), then
// blocks inside the rendering phase. With the basic render loop the scene graph runs on the
// GUI thread, so this hangs the render thread - which the quick-window watchdog has to kill.
ApplicationManagerWindow {
    id: win
    property bool blockNow: false

    Rectangle {
        anchors.centerIn: parent
        width: 50; height: 50
        color: "red"
        RotationAnimation on rotation {
            from: 0; to: 360; duration: 1000
            loops: Animation.Infinite
            running: true
        }
    }

    Timer {
        interval: 500
        running: true
        onTriggered: win.blockNow = true
    }

    // beforeSynchronizing has already set the render state to non-Idle by the time this fires,
    // so blocking here reliably leaves the window stuck in a render state.
    onBeforeRendering: {
        if (win.blockNow)
            while (true) ; // block the render thread forever
    }
}
