// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application
import NotifyHelper

ApplicationManagerWindow {
    color: "#888888"

    NotifyHelper {
        id: helper
    }

    // The test drives a notification from inside this app's process: it asks via an intent to post
    // a notification under a given app_name and replies with the resulting notification id. This
    // exercises the Notifications adaptor's caller-PID security check from a real app process.
    IntentHandler {
        intentIds: [ "notify-request" ]
        onRequestReceived: (request) => {
            let id = helper.notify(request.parameters.appName, request.parameters.summary);
            request.sendReply({ "id": id });
        }
    }
}
