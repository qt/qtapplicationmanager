// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

Item {
    width: 200
    height: 200

    IntentHandler {
        intentIds: [ "foreground" ]
        onRequestReceived: (request) => {
            IntentClient.broadcastIntentRequest("foreground-ack",
                                                { "from": ApplicationInterface.applicationId })
        }
    }
}
