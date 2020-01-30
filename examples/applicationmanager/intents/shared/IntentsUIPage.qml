/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:BSD-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: BSD-3-Clause
**
****************************************************************************/

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
