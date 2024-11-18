// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    id: root
    color: "lightgrey"

    Loader {
        source: "Compositor.qml"
        Text {
            anchors.fill: parent
            color: "red"
            font.bold: true
            text: "QtWaylandCompositor is not available"
            visible: !parent.item
        }
    }
}
