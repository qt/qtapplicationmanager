// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtApplicationManager.SystemUI 2.0

Item {
    width: 800
    height: 600

    // Show application names and icons
    Column {
        spacing: 20
        Repeater {
            model: ApplicationManager
            Column {
                id: delegate
                required property bool isRunning
                required property var icon
                required property var application
                required property string name
                Image {
                    source: delegate.icon
                    MouseArea {
                        anchors.fill: parent
                        onClicked: delegate.isRunning ? delegate.application.stop()
                                                      : delegate.application.start()
                    }
                }
                Text {
                    font.pixelSize: 20
                    text: delegate.name
                }
            }
        }
    }

    // Show windows
    Column {
        anchors.right: parent.right
        Repeater {
            model: WindowManager
            WindowItem {
                required property var model
                width: 600
                height: 200
                window: model.window
            }
        }
    }
}
