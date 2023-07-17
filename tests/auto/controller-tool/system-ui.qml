// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.SystemUI

Item {
    width: 800
    height: 600

    IntentServerHandler {
        intentIds: [ "inject-intent" ]
        onRequestReceived: (request) => {
            if (request.intentId === "inject-intent")
                request.sendReply({ "status": "ok" })
        }
    }

    Column {
        anchors.fill: parent
        Repeater {
            model: WindowManager
            WindowItem {
                required property var model
                width: 600
                height: 200
                window: model.window
            }
        }
    }
}
