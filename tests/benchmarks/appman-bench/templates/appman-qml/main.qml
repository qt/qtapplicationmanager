// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    Loader {
        anchors.fill: parent
        source: "../../test.qml"
    }
    Rectangle {
        width: 10
        height: 10
        color: "red"
        NumberAnimation on rotation {
            loops: Animation.Infinite
            from: 0
            to: 360
        }
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
            target.acknowledgeQuit();
        }
    }
}
