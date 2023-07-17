// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager 2.0
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root

    onWindowPropertyChanged: function(name, value) {
        if (name === "trigger" && value === "now")
            root.setWindowProperty("ack", "done");
    }

    Notification {
        id: notification
        priority: Notification.Low
        summary: "Test"
        body: "Body"
        category: "Category"
        icon: "file:///icon"
        image: "file:///image"
        actions: [ { "a1": "Action 1" }, { "a2": "Action 2" } ]
        showActionsAsIcons: false
        dismissOnAction: true
        acknowledgeable: true
        showProgress: true
        progress: 0.5
        sticky: true
        extended: { "key": 42 }

        onActionTriggered: (actionId) => {
            IntentClient.sendIntentRequest("notification-action", { "actionId": actionId })
        }
    }

    IntentHandler {
        intentIds: "test-window-property"
        onRequestReceived: function(request) {
            root.setWindowProperty("prop1", "bar");
            request.sendReply({ })
        }
    }
    IntentHandler {
        intentIds: "test-notification"
        onRequestReceived: function(request) {
            let funcReq = IntentClient.sendIntentRequest("system-func", { "str": "bar" })
            funcReq.onReplyReceived.connect(function() {
                if (funcReq.succeeded && funcReq.result["int"] === 42)
                    notification.show();
            })
            request.sendReply({ })
        }
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
            target.acknowledgeQuit();
        }
    }

    Component.onCompleted: {
        setWindowProperty("prop1", "foo");
    }
}
