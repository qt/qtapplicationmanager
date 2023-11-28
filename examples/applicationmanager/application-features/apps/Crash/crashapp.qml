// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.Application
import Crash
import Sequel

ApplicationManagerWindow {
    id: root

    readonly property var modes: ({
           illegalMemory: [ "Illegal memory access", root.accessIllegalMemory ],
           illegalMemoryInThread: [ "Illegal memory access in a thread", t2.accessIllegalMemoryInThread ],
           stackOverflow: [ "Force stack overflow", t2.forceStackOverflow ],
           divideByZero: [ "Divide by zero", t2.divideByZero ],
           raise: [ "Raise signal 7", t2.raise ],
           abort: [ "Call abort", Terminator1.abort ],
           unhandledException: [ "Throw unhandled exception", Terminator1.throwUnhandledException ],
           gracefully: [ "Exit gracefully", Terminator1.exitGracefully ]
    })

    property var accessIllegalMemory: (function() {
        let count = 0;
        return function recursive() {
            if (++count > 9)
                t2.accessIllegalMemory();
            else
                root.accessIllegalMemory();
        }
    })()

    color: "black"

    Terminator2 {
        id: t2
        signum: 7
    }

    CrashAnimation {
        id: crashAnimation;
        scaleTarget: content
        colorTarget: root
        onFinished: root.modes[mode][1]();
    }

    Grid {
        id: content
        anchors.centerIn: parent
        columns: 2
        Repeater {
            model: Object.keys(root.modes)
            Rectangle {
                required property var modelData
                width: root.width / 2
                height: root.height / 4
                border.width: 1
                color: "lightgrey"

                Text {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                    font.pointSize: 14
                    text: modes[parent.modelData][0]
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: crashAnimation.mode = parent.modelData;
                }
            }
        }
    }
}
