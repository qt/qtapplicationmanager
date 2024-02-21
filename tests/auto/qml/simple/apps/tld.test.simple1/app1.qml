// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    Rectangle {
        id: rect
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "red"
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
            // Casual check for attached ApplicationManagerWindow type
            if (rect.ApplicationManagerWindow.window.visible)
                target.acknowledgeQuit();
        }
    }
    IntentHandler {
        intentIds: [ "applicationInterfaceProperties" ]
        onRequestReceived: function(req) {
            req.sendReply({
                              "applicationId": ApplicationInterface.applicationId,
                              "applicationProperties": ApplicationInterface.applicationProperties,
                              "icon": ApplicationInterface.icon,
                              "systemProperties": ApplicationInterface.systemProperties,
                              "version": ApplicationInterface.version
                          })
        }
    }
}
