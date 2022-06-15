// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    visible: true

    Loader {
        id: ldr
        active: false
        source: "SubWin.qml"
    }

    Connections {
        target: ApplicationInterface
        function onOpenDocument(documentUrl) {
            switch (documentUrl) {
            case "show-sub": ldr.active = true; break;
            case "hide-sub": ldr.active = false; break;
            }
        }
    }
}
