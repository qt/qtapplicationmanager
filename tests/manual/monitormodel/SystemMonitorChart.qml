// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Frame {
    id: root

    property alias sourceModel: listView.model
    property string role

    signal removeClicked()

    ListView {
        id: listView
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: (root.width / root.sourceModel.count) * 0.2
        clip: true
        interactive: false

        property real maxValue: 0

        delegate: Item {
            width: (root.width / root.sourceModel.count) * 0.8
            height: root.height
            required property var model

            Rectangle {
                height: scaledValue * parent.height
                width: parent.width
                property real scaledValue: listView.maxValue > 0 ? value / listView.maxValue : 0
                readonly property real value: parent.model[root.role]
                onValueChanged: {
                    if (value > listView.maxValue)
                        listView.maxValue = value * 1.3;
                }
                y: parent.height - height
                color: root.palette.highlight
            }
        }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        text: root.role
    }

    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        text: "✖"
        flat: true
        onClicked: root.removeClicked()
    }
}
