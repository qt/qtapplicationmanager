// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application
import QtApplicationManager

ApplicationManagerWindow {
    id: root
    property string flavor: ApplicationInterface.applicationId.split(".")[1]

    IntentsUIPage {
        id: intentPage
        anchors.fill: parent
        flavor: root.flavor
        title: ApplicationInterface.name["en"]

        //! [Send Intent]
        onRequest: (intentId, applicationId, parameters) => {
            let request = IntentClient.sendIntentRequest(intentId, applicationId, parameters)
            request.onReplyReceived.connect(function() {
                intentPage.setResult(request.requestId, request.succeeded,
                                     request.succeeded ? request.result : request.errorMessage)
            })
        }
        //! [Send Intent]

        //! [Broadcast Intent]
        onBroadcast: (intentId, parameters) => {
            IntentClient.broadcastIntentRequest(intentId, parameters)
        }
        //! [Broadcast Intent]

        //! [Intent Animation]
        RotationAnimation on rotation {
            id: rotationAnimation
            running: false
            duration: 500; from: 0; to: 360
        }
        //! [Intent Animation]

        SequentialAnimation  on scale {
            id: scaleAnimation
            running: false
            PropertyAnimation { from: 1; to: 0.5 }
            PropertyAnimation { from: 0.5; to: 1 }
        }

        SequentialAnimation on color {
            id: blinkAnimation
            running: false
            ColorAnimation { to: "black" }
            ColorAnimation { to: "white" }
        }

        SequentialAnimation on color {
            id: blueAnimation
            running: false
            ColorAnimation { to: "blue"; duration: 2000 }
            ColorAnimation { to: "white" }
        }
    }

    //! [Intent Handler]
    IntentHandler {
        intentIds: "rotate-window"
        onRequestReceived: (request) => {
            rotationAnimation.start()
            request.sendReply({ "done": true })
        }
    }
    //! [Intent Handler]

    IntentHandler {
        intentIds: "scale-window"
        onRequestReceived: (request) => {
            scaleAnimation.start()
            request.sendReply({ "done": true })
        }
    }

    IntentHandler {
        intentIds: "blue-window-private"
        onRequestReceived: (request) => {
            blueAnimation.start()
            request.sendReply({ "done": true })
        }
    }

    IntentHandler {
        intentIds: "broadcast/blink-window"
        onRequestReceived: (request) => {
            blinkAnimation.start()
        }
    }
}
