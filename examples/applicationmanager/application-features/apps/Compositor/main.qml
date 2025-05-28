// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    color: "lightgrey"

    Text {
        color: "red"
        font.bold: true
        text: "QtWaylandCompositor is not available"
        visible: !ldr.item
    }

    Loader {
        id: ldr
        anchors.fill: parent
        source: "Compositor.qml"
    }

    onClosing: ldr.item?.close();
}
