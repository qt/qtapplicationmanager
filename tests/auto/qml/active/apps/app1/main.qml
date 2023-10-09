// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.Basic
import QtApplicationManager
import QtApplicationManager.Application
import QtTest


ApplicationManagerWindow {
    id: root
    width: 120
    height: 240
    visible: true
    title: "App 1"
    color: active ? "green" : "grey"

    Item {
        id: content
        objectName: "app1.content"
        anchors.fill: parent
        TextField {
            id: textField
            objectName: "app1.textField"
            width: content.width
            height: content.height / 3
            text: "Text App 1"
            focus: true
        }
        Button {
            id: button
            objectName: "app1.button"
            y: textField.height
            width: content.width
            height: content.height / 3
            text: "Btn App 1"
        }
    }

    SignalSpy {
        id: activeSpy
        target: root
        signalName: "activeChanged"
    }

    IntentHandler {
        intentIds: [ "activeFocusItem" ]
        onRequestReceived: (request) => {
            const waitUntilActive = request.parameters["waitUntilActive"] || 0
            const waitUntilInactive = request.parameters["waitUntilInactive"] || 0
            if ((waitUntilActive && !root.active) || (waitUntilInactive && root.active)) {
                activeSpy.clear()
                activeSpy.wait(waitUntilActive || waitUntilInactive)
            }

            request.sendReply({ "active": root.active,
                                "amwindow": root.activeFocusItem?.objectName,
                                "qwindow": content.Window.activeFocusItem?.objectName })
        }
    }
}
