// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    id: root
    color: mouseArea.pressed ? "red" : "peachpuff"

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "peru"

        Image {
            source: ApplicationInterface.icon
            anchors.centerIn: parent
        }

        RotationAnimation on rotation {
            from: 0; to: 360; loops: Animation.Infinite; duration: 4000
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
    }
}
