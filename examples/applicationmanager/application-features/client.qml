// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick

Rectangle {
    id: root

    width: 320
    height: 240
    color: "lightsteelblue"

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "orange"

        RotationAnimation on rotation {
            id: rotation
            from: 0; to: 360; loops: Animation.Infinite; duration: 2000
        }

        MouseArea {
            anchors.fill: parent
            onClicked: rotation.paused = !rotation.paused
        }
    }
}
