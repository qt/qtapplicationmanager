// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    color: "blue"

    Text {
        anchors.fill: parent
        text: "This a slim application that just animates."
        font.pixelSize: 15
        color: "white"
    }

    Rectangle {
        id: rectangle
        width: Math.min(parent.width, parent.height) / 2
        height: width
        anchors.centerIn: parent
        color: "grey"
        RotationAnimation {
            target: rectangle
            from: 0
            to: 360
            duration: 1000
            running: true
            loops: Animation.Infinite
        }

    }
}
