// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    color: ApplicationInterface.systemProperties.light ? "#B0808080" : "black"

    Image {
        anchors.centerIn: parent
        source: ApplicationInterface.icon

        MouseArea {
            anchors.fill: parent
            onClicked: {
                var notification = ApplicationInterface.createNotification();
                notification.summary = "Let there be light!"
                notification.show();
            }
        }
    }

    onWindowPropertyChanged: (name, value) => {
        console.log("App2: onWindowPropertyChanged - " + name + ": " + value);
    }

    Connections {
        target: ApplicationInterface
        function onOpenDocument(documentUrl, mimeType) {
            console.log("App2: onOpenDocument - " + documentUrl);
        }
        function onQuit() {
            ApplicationInterface.acknowledgeQuit();
        }
    }
}
