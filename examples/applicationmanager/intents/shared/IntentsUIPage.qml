// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.4

Rectangle {
    id: root
    property string flavor
    property string title: flavor

    function setResult(requestId, succeeded, result) {
        lblRequestId.text = requestId
        lblRequestStatus.text = succeeded ? "Result:" : "Error:"
        lblResult.text = JSON.stringify(result)
    }

    signal request(string intentId, string applicationId, var parameters)

    color: "white"
    Rectangle {
        opacity: 0.5
        anchors.fill: parent
        radius: 5
        border.width: 1
        border.color: root.color
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.flavor }
            GradientStop { position: 1.0; color: root.color }
        }
    }

    GridLayout {
        id: grid
        anchors.fill: parent
        anchors.margins: 5
        rowSpacing: 0
        columns: 2

        Label {
            text: root.title
            font.bold: true;
            horizontalAlignment: Text.AlignHCenter
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        Label { text: "Intent:" }
        ComboBox {
            id: cbIntent
            model: [ "rotate-window", "scale-window", "blink-window", "blue-window-private" ]
            Layout.fillWidth: true
        }
        Label { text: "Application:" }
        ComboBox {
            id: cbApplication
            model: [ "<not specified>", "intents.red", "intents.green", "intents.blue", ":sysui:" ]
            Layout.fillWidth: true
        }
        Button {
            id: btRequest
            text: "Request"
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter

            onClicked: {
                var appId = cbApplication.currentIndex === 0 ? "" : cbApplication.currentText
                root.request(cbIntent.currentText, appId, { "Request": { "Parameters": { "Testing": 1 }}})
            }
        }
        Label { text: "Request:" }
        Label { id: lblRequestId; Layout.fillWidth: true }
        Label { id: lblRequestStatus }
        Label { id: lblResult; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
