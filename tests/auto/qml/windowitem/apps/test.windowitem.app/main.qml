// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    id: root
    color: "red"

    width: 123
    height: 321

    MouseArea {
        id: mouseArea
        property int clickCount: 0
        anchors.fill: parent
        onClicked: {
            clickCount += 1;
            root.setWindowProperty("clickCount", clickCount);
        }
    }

    // A way for test code to trigger ApplicationManagerWindow's size changes from
    // the client side
    onWindowPropertyChanged: function(name, value) {
        if (name === "requestedWidth")
            root.width = value;
        else if (name === "requestedHeight")
            root.height = value;
    }

    Component.onCompleted: {
        root.setWindowProperty("clickCount", mouseArea.clickCount);
    }
}
