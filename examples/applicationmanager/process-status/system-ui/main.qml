// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick 2.4
import QtQuick.Controls 2.4
import QtApplicationManager.SystemUI 2.0

Pane {
    width: 900
    height: appsColumn.y + appsColumn.height

    // Show a warning on non-Linux platforms
    Rectangle {
        visible: Qt.platform.os !== 'linux'
        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        width: parent.width / 2
        height: warningLabel.height
        z: 1000
        color: "red"
        border.color: "white"
        Text {
            id: warningLabel
            width: parent.width
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "Please note that this example will not show sensible data on your current " +
                  "platform (" + Qt.platform.os + "). Only Linux/multi-process will produce " +
                  "all relevant data points."
        }
    }

    // Show name, icon and ProcessStatus data for each application
    Column {
        id: appsColumn
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 2
        spacing: 10
        Repeater {
            model: ApplicationManager
            ApplicationDisplay {
                required property var model
                name: model.name
                application: model.application
            }
        }
    }

    // Show windows of running applications
    Column {
        id: windowsColumn
        anchors.left: appsColumn.right
        anchors.right: parent.right
        anchors.margins: 10
        Repeater {
            model: WindowManager
            WindowItem {
                required property var model
                width: windowsColumn.width
                height: 200
                window: model.window
            }
        }
    }
}
