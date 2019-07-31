/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

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
