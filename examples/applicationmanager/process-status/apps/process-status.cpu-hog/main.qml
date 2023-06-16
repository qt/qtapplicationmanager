// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    color: "red"

    Text {
        anchors.fill: parent
        text: "This application consumes a lot of CPU time."
        font.pixelSize: 15
        color: "white"
    }

    Rectangle {
        id: rectangle
        width: Math.min(parent.width, parent.height) / 2
        height: width
        anchors.centerIn: parent
        color: "grey"
        Timer {
            interval: 10
            repeat: true
            running: true
            onTriggered: {
                rectangle.rotation += 1;
                doBusyWork();
            }
            function doBusyWork() {
                var i = 1;
                var n = 1;
                for (i = 1; i < 100000; i++) {
                    n = n + n * n / 1.5;
                }
            }
        }
    }
}
