// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    visible: false

    ApplicationManagerWindow {
        id: sub
        visible: false
        Component.onCompleted: setWindowProperty("type", "sub");
    }

    Rectangle {
        anchors.fill: parent
        visible: false

        ApplicationManagerWindow {
            id: sub2
            visible: false
            Component.onCompleted: setWindowProperty("type", "sub2");
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
            case "show-sub2": sub2.visible = true; break;
            case "hide-sub2": sub2.visible = false; break;
            }
        }
    }
}
