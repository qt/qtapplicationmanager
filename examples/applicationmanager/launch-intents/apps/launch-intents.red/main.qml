// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    color: "red"

    Text {
        id: txt

        property int counter: 0
        property int anotherCounter: 0

        anchors.centerIn: parent
        text: ApplicationInterface.name["en"] + "\n"
              + "Launch intents received: " + counter + " (normal)\n"
              + "Launch intents received: " + anotherCounter + " (another)"
    }

    IntentHandler {
        intentIds: [ "launch", "another-launch" ]
        onRequestReceived: (request) => {
            if (request.intentId === "launch")
                txt.counter++
            else
                txt.anotherCounter++
            request.sendReply({})
        }
    }
}
