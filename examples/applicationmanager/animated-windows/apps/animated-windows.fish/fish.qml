// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    color: mouseArea.pressed ? "red" : "darkblue"

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "aqua"

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
