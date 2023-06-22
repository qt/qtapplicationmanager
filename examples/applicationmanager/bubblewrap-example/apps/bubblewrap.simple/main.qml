// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtApplicationManager.Application

ApplicationManagerWindow {
    Rectangle {
        anchors.fill: parent
        border.color: "blue"

        Button {
            width: 200
            height: 50

            anchors.centerIn: parent

            text: "Click to exit with code 42"
            onClicked: Qt.exit(42)
        }
    }
}
