// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.12
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    color: "green"

    Text {
        id: txt

        property int counter: 0

        anchors.centerIn: parent
        text: ApplicationInterface.name["en"] + "\nLaunch intents received: " + counter
    }

    IntentHandler {
        intentIds: [ "launch" ]
        onRequestReceived: (request) => {
            txt.counter++
            request.sendReply({})
        }
    }
}
