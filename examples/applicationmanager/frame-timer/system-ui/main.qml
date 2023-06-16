// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtApplicationManager
import QtApplicationManager.SystemUI

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
                    required property var model
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
                    id: delegate
                    required property bool isRunning
                    required property var icon
                    required property var application
                    required property string name
                    Layout.alignment: Qt.AlignHCenter
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 100; implicitHeight: 100; radius: width/4
                        color: delegate.isRunning ? "darkgrey" : "lightgrey"
                        Image {
                            anchors.fill: parent
                            source: delegate.icon
                            sourceSize.width: 100
                            sourceSize.height: 100
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (delegate.isRunning)
                                    delegate.application.stop();
                                else
                                    delegate.application.start();
                            }
                        }
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: delegate.name + " application"
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
                required property int index
                required property WindowObject window

                width: 400; height: 320
                z: index
                color: "tan"

                // Title bar text
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: winChrome.window.application.names["en"] + " application window"
                }

                // Raises the window when the title bar is clicked and moves it around when dragged.
                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                    onPressed: windowsModel.move(winChrome.index, windowsModel.count-1, 1);
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
                        onClicked: winChrome.window.close()
                    }
                }

                // The window itself
                WindowItem {
                    id: windowItem
                    anchors.fill: parent
                    anchors.margins: 3
                    anchors.topMargin: 25
                    window: winChrome.window

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
                    running: winChrome.window && winChrome.window.contentState === WindowObject.SurfaceWithContent
                    window: winChrome.window
                }

                Component.onCompleted: {
                    winChrome.x =  300 + winChrome.index * 50;
                    winChrome.y =  10 + winChrome.index * 30;
                }

                readonly property bool shouldBeRemoved: winChrome.window && winChrome.window.contentState === WindowObject.NoSurface
                onShouldBeRemovedChanged: if (shouldBeRemoved) windowsModel.remove(winChrome.index, 1)
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
