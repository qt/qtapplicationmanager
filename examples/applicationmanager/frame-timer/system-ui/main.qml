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
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0

Window {
    id: sysuiWindow

    width: 1024
    height: 640

    Rectangle {
        anchors.fill: parent
        color: "linen"

        // Show a warning on non-Linux platforms
        Rectangle {
            visible: Qt.platform.os !== 'linux'
            anchors {
                bottom: parent.bottom
                horizontalCenter: parent.horizontalCenter
            }
            width: parent.width / 2
            height: warningLabel.height
            z: 1000
            color: "red"
            border.color: "white"
            Text {
                id: warningLabel
                width: parent.width
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: "Please note that this example will not show sensible data on your current " +
                      "platform (" + Qt.platform.os + "). Only Linux/multi-process will produce " +
                      "all relevant data points."
            }
        }

        // A graph displaying the FPS of the System UI itself
        Rectangle {
            id: fpsGraph
            anchors.top: parent.top
            anchors.right: parent.right
            width: 500
            height: 200
            color: "grey"
            ListView {
                id: listView
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: (fpsGraph.width / model.count) * 0.2
                clip: true
                interactive: false

                model: MonitorModel {
                    id: monitorModel
                    running: true
                    // Just put a FrameTimer inside a MonitorModel and you can use it as a data source
                    // There's no need to set its running property as the MonitorModel will take care of asking
                    // it to update itself when appropriate.
                    FrameTimer { window: sysuiWindow }
                }

                delegate: Rectangle {
                    width: (fpsGraph.width / monitorModel.count) * 0.8
                    height: (model.averageFps / 100) * fpsGraph.height
                    y: fpsGraph.height - height
                    color: "blue"
                }
            }

            Text {
                anchors.top: parent.top
                text: "100"
                font.pixelSize: 15
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "50"
                font.pixelSize: 15
            }

            Text {
                anchors.bottom: parent.bottom
                text: "0"
                font.pixelSize: 15
            }
        }


        // Application icons.
        // Click on an icon to start its application and click on it again to stop it.
        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 15
            Repeater {
                model: ApplicationManager
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 100; height: 100; radius: width/4
                        color: model.isRunning ? "darkgrey" : "lightgrey"
                        Image {
                            anchors.fill: parent
                            source: icon
                            sourceSize.width: 100
                            sourceSize.height: 100
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (model.isRunning)
                                    application.stop();
                                else
                                    application.start();
                            }
                        }
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: model.name + " application"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        // Application windows.
        // Each WindowObject is put in a WindowItem decorated with a border and a title bar.
        Repeater {
            model: ListModel { id: windowsModel }

            delegate: Rectangle {
                id: winChrome

                width: 400; height: 320
                z: model.index
                color: "tan"

                // Title bar text
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: model.window.application.name("en") + " application window"
                }

                // Raises the window when the title bar is clicked and moves it around when dragged.
                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                    onPressed: windowsModel.move(model.index, windowsModel.count-1, 1);
                }

                // Close button
                Rectangle {
                    width: 25; height: 25
                    color: "chocolate"
                    Text {
                        anchors.fill: parent
                        fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: 25
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        text: "x"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.window.close()
                    }
                }

                // The window itself
                WindowItem {
                    id: windowItem
                    anchors.fill: parent
                    anchors.margins: 3
                    anchors.topMargin: 25
                    window: model.window

                    Rectangle {
                        anchors.fill: fpsOverlay
                        color: "black"
                        opacity: 0.3
                    }

                    // And its FPS overlay
                    Column {
                        id: fpsOverlay
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 4
                        Text {
                            text: "averageFps: " + Number(frameTimer.averageFps).toLocaleString(Qt.locale("en_US"), 'f', 1)
                            font.pixelSize: 16; color: "white"
                        }
                        Text {
                            text: "maximumFps: " + Number(frameTimer.maximumFps).toLocaleString(Qt.locale("en_US"), 'f', 1)
                            font.pixelSize: 16; color: "white"
                        }
                        Text {
                            text: "minimumFps: " + Number(frameTimer.minimumFps).toLocaleString(Qt.locale("en_US"), 'f', 1)
                            font.pixelSize: 16; color: "white"
                        }
                        Text {
                            text: "jitterFps: " + Number(frameTimer.jitterFps).toLocaleString(Qt.locale("en_US"), 'f', 1)
                            font.pixelSize: 16; color: "white"
                        }
                    }
                }

                // FrameTimer will calculate the frame rate numbers for the given window
                FrameTimer {
                    id: frameTimer
                    // no sense in trying to update the FrameTimer while the window has no surface (or has just an empty one)
                    running: window && window.contentState === WindowObject.SurfaceWithContent
                    window: model.window
                }

                Component.onCompleted: {
                    winChrome.x =  300 + model.index * 50;
                    winChrome.y =  10 + model.index * 30;
                }

                readonly property bool shouldBeRemoved: model.window && model.window.contentState === WindowObject.NoSurface
                onShouldBeRemovedChanged: if (shouldBeRemoved) windowsModel.remove(model.index, 1)
            }
        }

        // Populates the windowsModel
        Connections {
            target: WindowManager
            function onWindowAdded(window) {
                windowsModel.append({"window":window})
            }
        }
    }
}
