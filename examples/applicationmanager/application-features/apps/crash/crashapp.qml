// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.11
import QtApplicationManager.Application 2.0
import Terminator 2.0

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
            model: Object.keys(methods)
            Rectangle {
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
                    text: methods[modelData]
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        switch (modelData) {
                        case "illegalMemory": accessIllegalMemory(); break;
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
