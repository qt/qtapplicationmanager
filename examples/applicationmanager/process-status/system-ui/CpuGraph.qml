// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtApplicationManager
import QtApplicationManager.SystemUI

/*
    This file shows how to use ProcessStatus inside a MonitorModel to draw a graph
 */
Pane {
    id: root

    property var application

    ListView {
        id: listView
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: (root.width / model.count) * 0.2
        clip: true
        interactive: false

        model: MonitorModel {
            id: monitorModel
            running: root.visible && root.application.runState === Am.Running
            ProcessStatus {
                applicationId: root.application.id
            }
        }

        delegate: Rectangle {
            required property var model
            width: (root.width / monitorModel.count) * 0.8
            height: model.cpuLoad * root.height
            y: root.height - height
            color: root.palette.highlight
        }
    }

    Label {
        anchors.top: parent.top
        text: "100%"
        font.pixelSize: 15
    }

    Label {
        anchors.verticalCenter: parent.verticalCenter
        text: "50%"
        font.pixelSize: 15
    }

    Label {
        anchors.bottom: parent.bottom
        text: "0%"
        font.pixelSize: 15
    }
}
