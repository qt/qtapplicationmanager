// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

QtObject {
    property var win1: ApplicationManagerWindow {
        color: "lightsteelblue"

        Rectangle {
            width: 80; height: 80; radius: 40
            color: "orange"

            MouseArea {
                anchors.fill: parent
                drag.target: parent
            }
        }
    }

    property var win2: ApplicationManagerWindow {
        color: "transparent"

        Rectangle {
            id: rect
            anchors.fill: parent
            color: "orange"
            opacity: 0.4
        }

        ApplicationManagerWindow {
            id: popup
            width: 300; height: 100

            Rectangle {
                anchors.fill: parent
                border.width: 1
                color: "orangered"
            }

            Text {
                anchors.centerIn: parent
                text: "Click to hide!"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: popup.visible = false;
            }

            Component.onCompleted: setWindowProperty("type", "pop-up");
        }
    }
}
