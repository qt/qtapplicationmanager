// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager
import QtApplicationManager.Application


ApplicationManagerWindow {
    id: root
    width: 320
    height: 240
    visible: true

    Rectangle {
        id: rect
        anchors.fill: parent
        color: "lightsteelblue"
        focus: true

        Text {
            text: `${rect.activeFocus}/${root.active}`
        }

        Text {
            id: txt
            anchors.centerIn: parent
            font.pixelSize: 32
        }

        Timer {
            id: tim
            onTriggered: txt.text = "";
        }

        Keys.onReleased: (event) => {
            IntentClient.sendIntentRequest("copythat", { app: 1, key: event.key });

            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                //console.info(`App1: ${event.key === Qt.Key_Up ? "UP" : "DOWN"}`);
                txt.text = `App1: ${event.key === Qt.Key_Up ? "UP" : "DOWN"}`;
                tim.restart();
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Left) {
                //console.info(`App1: ${event.key === Qt.Key_Right ? "RIGHT" : "LEFT"}`);
            } else {
                console.info(`App1: ${event.key}`);
            }
        }
    }
}
