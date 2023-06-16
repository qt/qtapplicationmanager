// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.Application
import Terminator

ApplicationManagerWindow {
    id: root

    property var methods: ({ illegalMemory: "Illegal memory access",
                             illegalMemoryInThread: "Illegal memory access in a thread",
                             stackOverflow: "Force stack overflow",
                             divideByZero: "Divide by zero",
                             unhandledException: "Throw unhandled exception",
                             abort: "Call abort",
                             raise: "Raise signal 7",
                             gracefully: "Exit gracefully" })


    function accessIllegalMemory()
    {
        Terminator.accessIllegalMemory();
    }

    Grid {
        columns: 2
        Repeater {
            model: Object.keys(root.methods)
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
                    text: root.methods[parent.modelData]
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        switch (parent.modelData) {
                        case "illegalMemory": root.accessIllegalMemory(); break;
                        case "illegalMemoryInThread": Terminator.accessIllegalMemoryInThread(); break;
                        case "stackOverflow": Terminator.forceStackOverflow(); break;
                        case "divideByZero": Terminator.divideByZero(); break;
                        case "unhandledException": Terminator.throwUnhandledException(); break;
                        case "abort": Terminator.abort(); break;
                        case "raise": Terminator.raise(7); break;
                        case "gracefully": Terminator.exitGracefully(); break;
                        }
                    }
                }
            }
        }
    }
}
