// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQml 2.2
import QtApplicationManager 2.0
import QtApplicationManager.Application 2.0

QtObject {
    id: root

    property var connections: Connections {
        target: ApplicationInterface
        function onQuit() {
            target.acknowledgeQuit();
        }
    }

    property var handler: IntentHandler {
        intentIds: [ "only1", "both", "match" ]
        onRequestReceived: {
            request.sendReply({ "from": ApplicationInterface.applicationId, "in": request.parameters})
        }
    }

    property var customErrorHandler: IntentHandler {
        intentIds: [ "custom-error" ]
        onRequestReceived: {
            request.sendErrorReply("custom error")
        }
    }
}
