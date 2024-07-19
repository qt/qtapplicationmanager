// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtApplicationManager.SystemUI
import QtApplicationManager
import shared

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
                    width: parent.width
                    font.pixelSize: 10
                    text: delegate.name
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

        IntentsUIPage {
            id: sysui_page
            width: parent.width / 2
            height: parent.height / 2
            title: "System UI"

            onRequest: (intentId, applicationId, parameters) => {
                let request = IntentClient.sendIntentRequest(intentId, applicationId, parameters)
                request.onReplyReceived.connect(function() {
                    sysui_page.setResult(request.requestId, request.succeeded,
                                         request.succeeded ? request.result : request.errorMessage)
                })
            }
            onBroadcast: (intentId, parameters) => {
                IntentClient.broadcastIntentRequest(intentId, parameters)
            }

            RotationAnimation on rotation {
                id: rotationAnimation
                running: false
                duration: 500; from: 0; to: 360
            }
            //! [IntentServerHandler]
            IntentServerHandler {
                intentIds: [ "rotate-window" ]
                names: { "en": "Rotate System UI" }
                visibility: IntentObject.Public

                onRequestReceived: (request) => {
                    rotationAnimation.start()
                    request.sendReply({ "wasRequestedBy": request.requestingApplicationId })
                }
            }
            //! [IntentServerHandler]
        }

        Repeater {
            model: WindowManager
            WindowItem {
                required property var model
                width: sysui_page.width
                height: sysui_page.height
                window: model.window
            }
        }
    }

    //! [Connection]
    Connections {
        target: IntentServer
        function onDisambiguationRequest(requestId, potentialIntents) {
            disambiguationDialog.add(requestId, potentialIntents)
        }
    }
    //! [Connection]

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

        //! [OkCancel]
        onAccepted: {
            IntentServer.acknowledgeDisambiguationRequest(currentRequest.requestId,
                                                          currentRequest.intents[handlingApplications.currentIndex]);
            showNext()
        }
        onRejected: {
            IntentServer.rejectDisambiguationRequest(currentRequest.requestId)
            showNext()
        }
        //! [OkCancel]

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
                    required property int index
                    required property var modelData
                    property ApplicationObject application: ApplicationManager.application(modelData.applicationId)
                    width: parent.width
                    text: modelData.name + " (" + modelData.applicationId + ")"
                    icon.source: modelData.icon
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
}
