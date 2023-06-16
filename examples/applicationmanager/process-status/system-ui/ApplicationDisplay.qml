// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.SystemUI
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property string name
    property var application

    width: 450

    RowLayout {
        width: parent.width

        Column {
            id: iconAndText

            Image {
                source: root.application.icon
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.application.runState === Am.Running ? application.stop() : application.start()
                }
            }
            Label {
                font.pixelSize: 18
                text: root.name
            }

        }

        Frame {
            opacity: root.application.runState === Am.Running ? 1 : 0
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    TabButton {
                        text: "Stats"
                        font.pixelSize: 15
                    }
                    TabButton {
                        text: "CPU Graph"
                        font.pixelSize: 15
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    currentIndex: tabBar.currentIndex
                    Stats {
                        application: root.application
                    }
                    CpuGraph {
                        application: root.application
                    }
                }
            }
        }

    }
}
