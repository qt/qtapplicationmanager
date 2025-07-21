// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    visible: false
    objectName: "root"

    Rectangle { id: rect; anchors.fill: parent; color: "green" }
    RotationAnimation {
        // this animation is a workaround for a (Qt)Wayland bug -- see test_amwin_close
        target: rect
        from: 0; to: 360; duration: 10000
        loops: Animation.Infinite
        running: true
    }

    ApplicationManagerWindow {
        id: sub
        visible: false
        objectName: "sub"
        Component.onCompleted: setWindowProperty("type", "sub");

        Rectangle { anchors.fill: parent; color: "red" }
    }

    Rectangle {
        anchors.fill: parent
        visible: false

        ApplicationManagerWindow {
            id: sub2
            visible: false
            objectName: "sub2"
            Component.onCompleted: setWindowProperty("type", "sub2");

            Rectangle { anchors.fill: parent; color: "yellow" }
        }
    }

    Connections {
        target: ApplicationInterface
        function onOpenDocument(documentUrl) {
            switch (documentUrl) {
            case "show-main": root.visible = true; break;
            case "hide-main": root.visible = false; break;
            case "show-sub": sub.visible = true; break;
            case "hide-sub": sub.visible = false; break;
            case "close-sub": sub.close(); break;
            case "show-sub2": sub2.visible = true; break;
            case "hide-sub2": sub2.visible = false; break;
            }
        }
    }
}
