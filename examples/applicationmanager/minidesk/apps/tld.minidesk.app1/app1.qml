// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    id: root
    color: "lightgrey"

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "lightsteelblue"

        Image {
            source: ApplicationInterface.icon
            anchors.centerIn: parent
        }

        RotationAnimation on rotation {
            id: rotation
            from: 0; to: 360; loops: Animation.Infinite; duration: 4000
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (rotation.paused) {
                    rotation.resume();
                } else {
                    rotation.pause();
                    root.setWindowProperty("rotation", parent.rotation);
                }
                popUp.visible = rotation.paused;
            }
        }
    }

    ApplicationManagerWindow {
        id: popUp
        visible: false
        color: "orangered"

        Text {
            anchors.centerIn: parent
            text: "App1 paused!"
        }

        Component.onCompleted: setWindowProperty("type", "pop-up");
    }
}
