// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

QtObject {
    id: root

    property ApplicationManagerWindow mainWin: ApplicationManagerWindow {
        id: main
        visible: false
    }

    property ApplicationManagerWindow subWin: ApplicationManagerWindow {
        id: sub
        visible: false
        Component.onCompleted: setWindowProperty("type", "sub");
    }

    property Connections con: Connections {
        target: ApplicationInterface
        function onOpenDocument(documentUrl) {
            switch (documentUrl) {
                case "show-main": main.visible = true; break;
                case "hide-main": main.visible = false; break;
                case "show-sub": sub.visible = true; break;
                case "hide-sub": sub.visible = false; break;
            }
        }
    }
}
