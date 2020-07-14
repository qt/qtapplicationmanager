/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

import QtQuick 2.11
import QtQuick.Window 2.11
import QtApplicationManager.SystemUI 2.0

Window {
    title: "Minidesk - QtApplicationManager Example"
    width: 1024
    height: 640
    color: "whitesmoke"

    Readme {}

    Text {
        anchors.bottom: parent.bottom
        text: (ApplicationManager.singleProcess ? "Single" : "Multi") + "-Process Mode"
    }

    // Application launcher panel
    Column {
        Repeater {
            model: ApplicationManager

            Image {
                source: icon
                opacity: isRunning ? 0.3 : 1.0

                MouseArea {
                    anchors.fill: parent
                    onClicked: isRunning ? application.stop() : application.start("documentUrl");
                }
            }
        }
    }

    // System UI chrome for applications
    Repeater {
        model: ListModel { id: topLevelWindowsModel }

        delegate: Image {
            source: "chrome-bg.png"
            z: model.index

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Decoration: " + (model.window.application ? model.window.application.name("en")
                                                                 : 'External Application')
            }

            MouseArea {
                anchors.fill: parent
                drag.target: parent
                onPressed: topLevelWindowsModel.move(model.index, topLevelWindowsModel.count - 1, 1);
            }

            Rectangle {
                width: 25; height: 25
                color: "chocolate"

                MouseArea {
                    anchors.fill: parent
                    onClicked: model.window.close();
                }
            }

            WindowItem {
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
                window: model.window

                Connections {
                    target: window
                    function onContentStateChanged() {
                        if (window.contentState === WindowObject.NoSurface)
                            topLevelWindowsModel.remove(model.index, 1);
                    }
                }
            }

            Component.onCompleted: {
                x = 300 + model.index * 50;
                y =  10 + model.index * 30;
            }
        }
    }

    // System UI for a pop-up
    WindowItem {
        id: popUpContainer
        z: 9998
        width: 200; height: 60
        anchors.centerIn: parent

        Connections {
            target: popUpContainer.window
            function onContentStateChanged() {
                if (popUpContainer.window.contentState === WindowObject.NoSurface) {
                    popUpContainer.window = null;
                }
            }
        }
    }

    // System UI for a notification
    Text {
        z: 9999
        font.pixelSize: 46
        anchors.centerIn: parent
        text: NotificationManager.count > 0 ? NotificationManager.get(0).summary : ""
    }

    // Handler for WindowManager signals
    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            if (window.windowProperty("type") === "pop-up") {
                popUpContainer.window = window;
            } else {
                topLevelWindowsModel.append({"window": window});
                window.setWindowProperty("propA", 42);
            }
        }

        function onWindowPropertyChanged(window, name, value) {
            console.log("SystemUI: OnWindowPropertyChanged [" + window + "] - " + name + ": " + value);
        }
    }

    // IPC extension
    ApplicationIPCInterface {
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

        Component.onCompleted: ApplicationIPCManager.registerInterface(this, "tld.minidesk.interface", {});
    }
}
