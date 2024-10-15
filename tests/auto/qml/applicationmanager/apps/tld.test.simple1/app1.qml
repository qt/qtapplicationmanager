// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.Basic
import QtApplicationManager.Application
import QtTest

ApplicationManagerWindow {
    id: root

    title: "Test"
    width: 180
    height: 180
    visible: true
    color: active ? "green" : "grey"

    TextField {
        id: text
        focus: true
        anchors.centerIn: parent
        text: "Test"
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
            ApplicationInterface.acknowledgeQuit();
        }
    }

    SignalSpy {
        id: activeSpy
        target: root
        signalName: "activeChanged"
    }

    IntentHandler {
        intentIds: [ "applicationManagerWindow" ]
        onRequestReceived: function(req) {
            const waitUntilActive = req.parameters["waitUntilActive"] || 0
            if (waitUntilActive && !root.active) {
                activeSpy.clear()
                activeSpy.wait(waitUntilActive)
            }

            let bo = root.backingObject
            let boWindow = bo as Window
            let boItem = bo as Item
            let boOk = root.singleProcess ? (boItem !== null && boWindow === null)
                                          : (boItem === null && boWindow !== null)

            let attachedDataOk =
                (text.ApplicationManagerWindow.backingObject === bo) &&
                (text.ApplicationManagerWindow.visibility === root.visibility) &&
                (text.ApplicationManagerWindow.active === root.active) &&
                (text.ApplicationManagerWindow.activeFocusItem === root.activeFocusItem) &&
                (text.ApplicationManagerWindow.contentItem === root.contentItem) &&
                (text.ApplicationManagerWindow.width === root.width) &&
                (text.ApplicationManagerWindow.height === root.height)

            let reply = {
                "attachedItemOk": text.ApplicationManagerWindow.window === root,
                "selfAttachedItemOk": root.ApplicationManagerWindow.window === root,
                "attachedDataOk": attachedDataOk,
                "singleProcess": root.singleProcess,
                "backingObject": boOk,
                "color": root.color === Qt.color("green"),
                "contentItem": root.contentItem !== null,
                "title": root.title,
                "x": root.x,
                "y": root.y,
                "width": root.width,
                "height": root.height,
                "minimumWidth": root.minimumWidth,
                "minimumHeight": root.minimumHeight,
                "maximumWidth": root.maximumWidth,
                "maximumHeight": root.maximumHeight,
                "visible": root.visible,
                "active": root.active,
                "visibility": root.visibility,
                "opacity": root.opacity,
                "activeFocusItem": root.activeFocusItem === text,
            }
            req.sendReply(reply)
        }
    }

    IntentHandler {
        intentIds: [ "applicationInterfaceProperties" ]
        onRequestReceived: function(req) {
            let reply = {
                "applicationId": ApplicationInterface.applicationId,
                "applicationProperties": ApplicationInterface.applicationProperties,
                "icon": ApplicationInterface.icon,
                "systemProperties": ApplicationInterface.systemProperties,
                "version": ApplicationInterface.version
            }
            req.sendReply(reply)
        }
    }

    IntentHandler {
        intentIds: [ "qapplicationProperties" ]
        onRequestReceived: function(req) {
            let reply = {
                "name": Application.name,
                "domain": Application.domain,
                "organization": Application.organization,
                "version": Application.version
            }
            req.sendReply(reply)
        }
    }
}
