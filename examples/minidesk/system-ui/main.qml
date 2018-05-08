/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:BSD-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: BSD-3-Clause
**
****************************************************************************/

import QtQuick 2.4
import QtApplicationManager 1.0

Rectangle {
    width: 1024
    height: 640
    color: "linen"

    Readme {}

    Text {
        anchors.bottom: parent.bottom
        text: (ApplicationManager.singleProcess ? "Single" : "Multi") + "-Process Mode"
    }

    // Application launcher panel
    Column {
        id: launcher
        Repeater {
            id: menuItems
            model: ApplicationManager

            Image {
                source: icon
                opacity: isRunning ? 0.3 : 1.0

                MouseArea {
                    anchors.fill: parent
                    onClicked: application.start();
                }
            }
        }
    }

    // System-UI chrome for applications
    Repeater {
        model: ListModel { id: topLevelWindowsModel }

        delegate: Rectangle {
            id: winChrome

            width: 400; height: 320
            z: model.index
            color: "tan"

            property bool manuallyClosed: false

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Decoration: " + model.window.application.name("en")
            }

            MouseArea {
                anchors.fill: parent
                drag.target: parent
                onPressed: topLevelWindowsModel.move(model.index, topLevelWindowsModel.count-1, 1);
            }

            Rectangle {
                width: 25; height: 25
                color: "chocolate"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        winChrome.manuallyClosed = true;
                        model.window.application.stop();
                    }
                }
            }

            WindowItem {
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
                window: model.window
            }

            Component.onCompleted: {
                winChrome.x =  300 + model.index * 50;
                winChrome.y =  10 + model.index * 30;
            }

            states: [
                State {
                    name: "open"
                    when: model.window && model.window.contentState === WindowObject.SurfaceWithContent && !manuallyClosed
                    PropertyChanges {
                        target:  winChrome
                        opacity: 1
                        scale: 1
                        visible: true
                    }
                }
            ]

            opacity: 0.25
            scale: 0.50
            visible: false

            transitions: [
                Transition {
                    to: "open"
                    NumberAnimation { target: winChrome; properties: "opacity,scale"; duration: 500; easing.type: Easing.OutQuad}
                },
                Transition {
                    from: "open"
                    SequentialAnimation {
                        PropertyAction { target: winChrome; property: "visible"; value: true } // we wanna see the window during the closing animation
                        NumberAnimation { target: winChrome; properties: "opacity,scale"; duration: 500; easing.type: Easing.InQuad}
                        ScriptAction { script: {
                            if (model.window.contentState === WindowObject.NoSurface)
                                topLevelWindowsModel.remove(model.index, 1);
                        } }
                    }
                }
            ]
        }
    }

    // System-UI for a pop-up and notification
    Repeater {
        model: ListModel { id: popupsModel }
        delegate: WindowItem {
            z: 100 + model.index
            width: 200; height: 60
            anchors.centerIn: parent
            window: model.window
            Connections {
                target: model.window
                onContentStateChanged: {
                    if (model.window.contentState === WindowObject.NoSurface)
                        popupsModel.remove(model.index, 1);
                }
            }
        }
    }

    Text {
        z: 30001
        font.pixelSize: 46
        anchors.centerIn: parent
        text: NotificationManager.count > 0 ? NotificationManager.get(0).summary : ""
    }

    // Handler for WindowManager signals
    Connections {
        target: WindowManager
        onWindowAdded:  {
            // separate windows by their type, adding them to their respective model
            var model = window.windowProperty("type") === "pop-up" ? popupsModel : topLevelWindowsModel;
            model.append({"window":window});
        }
    }

    // IPC extension
    ApplicationIPCInterface {
        id: extension

        property double pi
        signal computed(string what)
        readonly property var _decltype_circumference: { "double": [ "double", "string" ] }
        function circumference(radius, thing) {
            console.log("SystemUI: circumference(" + radius + ", \"" + thing + "\") has been called");
            pi = 3.14;
            var circ = 2 * pi * radius;
            computed("circumference for " + thing);
            return circ;
        }

        Component.onCompleted: ApplicationIPCManager.registerInterface(extension, "tld.minidesk.interface", {});
    }
}
