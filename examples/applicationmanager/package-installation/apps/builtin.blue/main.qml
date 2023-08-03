// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    Rectangle {
        anchors.fill: parent
        border.color: "blue"
        border.width: 20
        Text {
            anchors.centerIn: parent
            text: "Built-in: " + ApplicationInterface.name.en
        }
    }
}
