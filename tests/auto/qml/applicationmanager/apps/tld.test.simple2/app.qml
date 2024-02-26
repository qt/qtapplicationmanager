// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "red"
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
           //Do nothing, so we get killed by appman
        }
    }
}
