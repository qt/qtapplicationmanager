// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.Application
import QtQuick.Controls
import Glitches

ApplicationManagerWindow {
    id: root

    readonly property var modes: ({
        sleepGui: { name: "Sleep GUI", method: glitches.sleepGui },
        sleepSync: { name: "Sleep sync", method: glitches.sleepSync },
        sleepRender: { name: "Sleep render", method: glitches.sleepRender }
    })

    component NamedRect: Text {
        property alias rect: rect
        width: 120; height: 120
        horizontalAlignment: Text.AlignHCenter

        Rectangle {
            id: rect
            anchors.centerIn: parent
            width: 50; height: 50; radius: width/8
            color: parent.color
        }
    }

    FontMetrics {
        id: fontMetrics
    }

    Glitches {
        id: glitches
        duration: tumbler.currentIndex / 5
    }

    Row {
        spacing: 15
        topPadding: 20

        Tumbler {
            id: tumbler
            height: buttons.height
            model: 31
            currentIndex: 10
            wrap: false

            delegate: Rectangle {
                required property int index

                Text {
                    anchors.centerIn: parent
                    text: (index / 5.0).toFixed(1)
                    font.bold: index === tumbler.currentIndex
                    font.pixelSize: fontMetrics.font.pixelSize * (4 - Math.abs(tumbler.currentIndex - index)) * 0.4
                }
            }
        }

        Column {
            id: buttons
            spacing: 5

            Repeater {
                model: Object.keys(root.modes)

                Button {
                    required property var modelData

                    width: 120; height: 40
                    text: `${modes[modelData].name} ${(tumbler.currentIndex / 5.0).toFixed(1)}s`
                    onClicked: modes[modelData].method();
                }
            }
        }

        NamedRect {
            id: animation
            text: "Animation"
            color: "darkorange"

            RotationAnimation on rotation {
                target: animation.rect; from: 0; to: 360; loops: Animation.Infinite; duration: 4000
            }
        }

        NamedRect {
            id: animator
            text: "Animator"
            color: "deepskyblue"

            RotationAnimator on rotation {
                id: rotationAnimator
                target: animator.rect; from: 0; to: 360; loops: Animation.Infinite; duration: 4000; running: true
            }
        }
    }
}
