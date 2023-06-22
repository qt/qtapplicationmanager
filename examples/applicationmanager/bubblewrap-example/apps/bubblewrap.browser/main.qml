// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    Rectangle {
        anchors.fill: parent
        border.color: "green"

        Text {
            visible: loader.status === Loader.Error
            anchors.centerIn: parent
            text: "Can't load QtWebEngine, please make sure it is installed."
        }

        Loader {
            id: loader
            anchors.fill: parent
            anchors.margins: 2
            source: "Browser.qml"
        }
    }
}
