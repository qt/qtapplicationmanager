// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtApplicationManager.SystemUI

Window {
    title: "Application Features - QtApplicationManager Example"
    width: 1280
    height: 800
    color: "whitesmoke"

    Text {
        anchors.bottom: parent.bottom
        text: (ApplicationManager.singleProcess ? "Single" : "Multi") + "-Process Mode"
    }

    Column {
        z: 9998
        Repeater {
            model: ApplicationManager

            Image {
                id: delegate
                required property var application
                required property bool isRunning
                required property var icon
                source: icon
                opacity: isRunning ? 0.3 : 1.0

                Rectangle {
                    x: 96; y: 38
                    width: appid.width; height: appid.height
                    color: "whitesmoke"
                    visible: imouse.containsMouse

                    Text {
                        id: appid
                        text: delegate.application.names["en"]
                    }
                }

                MouseArea {
                    id: imouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: delegate.isRunning ? delegate.application.stop()
                                                  : delegate.application.start();
                }
            }
        }
    }

    Repeater {
        model: ListModel { id: topLevelWindowsModel }

        delegate: Rectangle {
            id: chrome
            required property WindowObject window
            required property int index
            width: draggrab.x + 10; height: draggrab.y + 10
            color: "transparent"
            border.width: 3
            border.color: "grey"
            z: index

            Image {
                id: draggrab
                x: 710; y: 530
                source: "grab.png"
                visible: dragarea.containsMouse
            }

            MouseArea {
                id: dragarea
                anchors.fill: draggrab
                hoverEnabled: true
                drag.target: draggrab
                drag.minimumX: 230
                drag.minimumY: 170
            }

            Rectangle {
                height: 25
                width: parent.width
                color: "grey"

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: chrome.window.application ? chrome.window.application.names["en"]
                                                    : 'External Application'
                }

                MouseArea {
                    anchors.fill: parent
                    drag.target: chrome
                    onPressed: topLevelWindowsModel.move(chrome.index, topLevelWindowsModel.count - 1, 1);
                }

                Rectangle {
                    width: 25; height: 25
                    color: "chocolate"

                    MouseArea {
                        anchors.fill: parent
                        onClicked: chrome.window.close();
                    }
                }
            }

            WindowItem {
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
                window: chrome.window

                Connections {
                    target: chrome.window
                    function onContentStateChanged() {
                        if (chrome.window.contentState === WindowObject.NoSurface)
                            topLevelWindowsModel.remove(chrome.index, 1);
                    }
                }
            }

            Component.onCompleted: {
                x = 200 + chrome.index * 50;
                y =  20 + chrome.index * 30;
            }
        }
    }

    Repeater {
        model: ListModel { id: popupsModel }
        delegate: WindowItem {
            id: win
            required property var model
            z: 9999 + model.index
            anchors.centerIn: parent
            window: model.window
            Connections {
                target: win.model.window
                function onContentStateChanged() {
                    if (win.model.window.contentState === WindowObject.NoSurface)
                        popupsModel.remove(win.model.index, 1);
                }
            }
        }
    }

    Text {
        z: 9999
        font.pixelSize: 46
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: NotificationManager.count > 0 ? NotificationManager.get(0).summary : ""
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            var model = window.windowProperty("type") === "pop-up" ? popupsModel : topLevelWindowsModel;
            model.append({"window":window});
        }
    }

    Connections {
        target: ApplicationManager
        function onApplicationRunStateChanged(id, runState) {
            if (runState === Am.NotRunning
                && ApplicationManager.application(id).lastExitStatus === Am.CrashExit) {
                ApplicationManager.startApplication(id);
            }
        }
    }
}
