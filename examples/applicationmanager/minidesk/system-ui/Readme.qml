// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick

Item {
    anchors.fill: parent

    Column {
        width: paragraph.implicitWidth
        height: heading.implicitHeight + paragraph.implicitHeight + 80
        anchors.centerIn: parent
        spacing: 10

        Text {
            id: heading
            color: "cornflowerblue"
            font { pixelSize: 20; weight: Font.Bold }
            text: "Minimal Desktop System UI in pure QML"
        }

        Text {
            id: paragraph
            color: "grey"
            font.pixelSize: 16
            text: "The following features are supported:\n" +
                  "\u2022 Start applications by clicking on an icon in the top left\n" +
                  "\u2022 Stop an application by clicking on the icon in the top left again\n" +
                  "\u2022 Close an application window by clicking on the window's close icon\n" +
                  "\u2022 Raise a window and set focus by pressing on the window (focus window is blue)\n" +
                  "\u2022 Drag a window by pressing on the top window decoration and moving them\n" +
                  "\u2022 The System UI sends a 'propA' change when an app starts\n" +
                  "\u2022 The System UI and App2 react to window property changes with a debug message\n" +
                  "\u2022 Stop or restart App1 animations with a click\n" +
                  "\u2022 App1 sends rotation angle as a window property to System UI on stop\n" +
                  "\u2022 App1 shows a pop-up on the System UI while it is paused\n" +
                  "\u2022 App2 logs the document URL that started it\n" +
                  "\u2022 App2 triggers a notification in the System UI when the bulb icon is clicked\n" +
                  "\u2022 Show Wayland client windows that originate from processes outside of appman"
        }
    }
}
