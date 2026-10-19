// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    width: 200
    height: 200

    IntentHandler {
        intentIds: [ "foreground" ]
        onRequestReceived: (request) => {
            IntentClient.broadcastIntentRequest("foreground-ack",
                                                { "from": ApplicationInterface.applicationId })
        }
    }

    // The window's componentComplete() has already run (and thus the window got added) when the
    // children's onCompleted handlers are called. Exceeding the incubation controller's time slice
    // here forces the incubator to return to the event loop before the incubation is finished.
    Item {
        Component.onCompleted: {
            const start = Date.now()
            while (Date.now() - start < 100) { }
        }
    }
}
