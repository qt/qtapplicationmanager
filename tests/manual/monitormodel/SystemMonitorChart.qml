// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.11
import QtQuick.Controls 2.4

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

        delegate: Rectangle {
            width: (root.width / root.sourceModel.count) * 0.8
            height: scaledValue * root.height
            property real scaledValue: listView.maxValue > 0 ? value / listView.maxValue : 0
            readonly property real value: model[root.role]
            onValueChanged: {
                if (value > listView.maxValue)
                    listView.maxValue = value * 1.3;
            }
            y: root.height - height
            color: root.palette.highlight
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
