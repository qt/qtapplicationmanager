/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
import QtApplicationManager 1.0
import "shared"

Item {
    id: root
    width: 900
    height: 600

    // Show application names and icons
    Column {
        id: launcher
        width: 100
        spacing: 20
        Repeater {
            model: ApplicationManager
            Column {
                Image {
                    source: model.icon
                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.isRunning ? application.stop() : application.start()
                    }
                }
                Text {
                    width: parent.width
                    font.pixelSize: 10
                    text: model.name
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // Show windows
    Grid {
        anchors {
            right: parent.right
            left: launcher.right
            top: parent.top
            bottom: parent.bottom
        }
        columns: 2

        IntentPage {
            id: sysui_page
            width: parent.width / 2
            height: parent.height / 2
            title: "System-UI"

            onRequest: {
                var request = IntentClient.createIntentRequest(intentId, applicationId, parameters)
                request.onFinished.connect(function() {
                    sysui_page.setResult(request.requestId, request.succeeded,
                                         request.succeeded ? request.result : request.errorMessage)
                })
            }
        }

        Repeater {
            model: WindowManager
            WindowItem {
                width: sysui_page.width
                height: sysui_page.height
                window: model.window
            }
        }
    }


    Dialog {
        id: disambiguationDialog
        title: "Disambiguation"
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.NoAutoClose
        modal: true

        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: parent.width / 3 * 2
        height: parent.height / 3 * 2

        property var allRequests: []
        property var currentRequest: ({})

        function add(requestId, potentialIntents) {
            allRequests.push({ "requestId": requestId, "intents": potentialIntents})

            if (!visible)
                showNext()
        }
        function showNext() {
            currentRequest = {}
            if (allRequests.length !== 0) {
                currentRequest = allRequests.pop()
                intentRequest.text = currentRequest.requestId
                intent.text = currentRequest.intents[0].intentId
                handlingApplications.model = currentRequest.intents
                open()
            }
        }
        onAccepted: {
            IntentServer.acknowledgeDisambiguationRequest(currentRequest.requestId,
                                                          currentRequest.intents[handlingApplications.currentIndex]);
            showNext()
        }
        onRejected: {
            IntentServer.rejectDisambiguationRequest(currentRequest.requestId)
            showNext()
        }

        GridLayout {
            id: grid
            anchors.fill: parent
            anchors.margins: 5
            rowSpacing: 5
            columns: 2

            Label { text: "Request Id:" }
            Label { id: intentRequest; font.bold: true; Layout.fillWidth: true }
            Label { text: "Intent Id:" }
            Label { id: intent; font.bold: true; Layout.fillWidth: true }

            Label {
                text: "Select one of the potential handler applications:"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignCenter
            }

            ListView {
                id: handlingApplications

                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.fillHeight: true

                delegate: ItemDelegate {
                    property ApplicationObject application: ApplicationManager.application(modelData.applicationId)
                    width: parent.width
                    text: application.name("en") + " (" + modelData.applicationId + ")"
                    icon.source: application.icon
                    icon.color: "transparent"
                    highlighted: ListView.isCurrentItem
                    onClicked: { handlingApplications.currentIndex = index }
                }
                model: []
                clip: true
                ScrollIndicator.vertical: ScrollIndicator { }
            }
        }
    }

    Connections {
        target: IntentServer
        onDisambiguationRequest: { disambiguationDialog.add(requestId, potentialIntents) }
    }
}
