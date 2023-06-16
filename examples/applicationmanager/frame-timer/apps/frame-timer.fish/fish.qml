// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    id: root
    color: mouseArea.pressed ? "red" : "darkblue"

    Rectangle {
        id: rectangle
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "aqua"

        Image {
            source: ApplicationInterface.icon
            anchors.centerIn: parent
        }

        Timer {
            running: true
            repeat: true
            interval: 1000 / 25 // 25 frames per second
            onTriggered: {
                rectangle.rotation = (rectangle.rotation + 5) % 360;
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
    }
}
