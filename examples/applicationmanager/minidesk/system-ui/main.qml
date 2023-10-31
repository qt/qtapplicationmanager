// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.SystemUI

Window {
    title: "Minidesk - QtApplicationManager Example"
    width: 1024
    height: 640
    color: "whitesmoke"

    Readme {}

    Text {
        anchors.bottom: parent.bottom
        text: `${ApplicationManager.singleProcess ? "Single" : "Multi"}-process mode  |  ${PackageManager.architecture}`
    }

    // Application launcher panel
    Column {
        Repeater {
            model: ApplicationManager

            Image {
                required property string icon
                required property bool isRunning
                required property ApplicationObject application
                source: icon
                opacity: isRunning ? 0.3 : 1.0

                MouseArea {
                    anchors.fill: parent
                    onClicked: parent.isRunning ? parent.application.stop() : parent.application.start("documentUrl");
                }
            }
        }
    }

    // System UI chrome for applications
    Repeater {
        model: ListModel { id: topLevelWindowsModel }

        delegate: FocusScope {
            id: winChrome
            required property int index
            required property WindowObject window

            width: chromeImg.width
            height: chromeImg.height
            z: index

            Image {
                id: chromeImg
                source: winChrome.activeFocus ? "chrome-active.png" : "chrome-bg.png"

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (25 - contentHeight) / 2
                    color: "white"
                    text: "Decoration: " + (winChrome.window.application?.names["en"] ?? 'External Application')
                }

                MouseArea {
                    width: chromeImg.width; height: 25
                    drag.target: winChrome
                    onPressed: winItem.forceActiveFocus();
                }

                Image {
                    source: "close.png"
                    MouseArea {
                        anchors.fill: parent
                        onClicked: winChrome.window.close();
                    }
                }
            }

            WindowItem {
                id: winItem
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
                window: winChrome.window

                Connections {
                    target: winChrome.window
                    function onContentStateChanged() {
                        if (winChrome.window.contentState === WindowObject.NoSurface)
                            topLevelWindowsModel.remove(winChrome.index, 1);
                    }
                }
            }

            onFocusChanged: (focus) => {
                if (focus)
                    topLevelWindowsModel.move(winChrome.index, topLevelWindowsModel.count - 1, 1);
            }

            Component.onCompleted: {
                x = 300 + winChrome.index * 50;
                y =  10 + winChrome.index * 30;
                winItem.forceActiveFocus();
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
                if (popUpContainer.window.contentState === WindowObject.NoSurface)
                    popUpContainer.window = null;
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
}
