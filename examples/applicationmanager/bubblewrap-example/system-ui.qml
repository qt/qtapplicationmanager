// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtApplicationManager.SystemUI 2.0

Item {
    width: 800
    height: 600

    // Show application names and icons
    Column {

        Repeater {
            model: ApplicationManager
            Item {
                width: 200
                height: 200

                Image {
                    id: icon
                    source: model.icon
                    anchors.centerIn: parent
                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.isRunning ? application.stop() : application.start()
                    }
                }
                Text {
                    font.pixelSize: 15
                    text: model.name
                    anchors.top: icon.bottom
                    anchors.topMargin: 5
                    anchors.horizontalCenter: icon.horizontalCenter
                }
            }
        }
    }

    // Show windows
    Column {
        anchors.right: parent.right
        spacing: 1

        Repeater {
            model: WindowManager
            WindowItem {
                width: 600
                height: 200
                window: model.window
            }
        }
    }
}
