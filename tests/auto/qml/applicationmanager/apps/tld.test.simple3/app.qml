// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    id: root

    Rectangle {
        anchors.centerIn: parent
        width: 180; height: 180; radius: width/4
        color: "yellow"
    }
    // no connection to onQuit
}
