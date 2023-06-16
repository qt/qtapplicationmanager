// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.SystemUI

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
                id: delegate
                required property bool isRunning
                required property var icon
                required property var application
                source: icon

                Text {
                    anchors.fill: parent
                    fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: height
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: delegate.isRunning ? "Stop" : "Start"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (delegate.isRunning)
                            delegate.application.stop()
                        else
                            delegate.application.start()
                    }
                }
            }
        }
    }

    // System UI chrome for applications
    Repeater {
        model: ListModel { id: topLevelWindowsModel }

        delegate: Rectangle {
            id: winChrome
            required property WindowObject window
            required property int index

            width: 400; height: 320
            z: index
            color: "tan"

            property bool manuallyClosed: false

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (windowItem.primary ? "Primary: " : "Secondary: ") + winChrome.window.application.names["en"]
            }

            MouseArea {
                anchors.fill: parent
                drag.target: parent
                onPressed: topLevelWindowsModel.move(winChrome.index, topLevelWindowsModel.count-1, 1)
            }

            Rectangle {
                width: 25; height: 25
                color: "chocolate"
                Text {
                    anchors.fill: parent
                    fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: 25
                    horizontalAlignment: Text.AlignHCenter
                    text: "-"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        winChrome.manuallyClosed = true
                    }
                }
            }

            Text {
                width: 25; height: 25
                anchors.right: addWindowItemButton.left
                fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: 25

                text: "P"
                visible: !windowItem.primary

                MouseArea {
                    anchors.fill: parent
                    onClicked: windowItem.makePrimary()
                }
            }
            Text {
                id: addWindowItemButton
                width: 25; height: 25
                anchors.right: parent.right
                fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: 25

                text: "+"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        topLevelWindowsModel.append({"window": winChrome.window})
                    }
                }
            }

            WindowItem {
                id: windowItem
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
                window: winChrome.window
            }

            Component.onCompleted: {
                winChrome.x =  300 + winChrome.index * 50
                winChrome.y =  10 + winChrome.index * 30
            }

            states: [
                State {
                    name: "open"
                    when: winChrome.window
                          && winChrome.window.contentState === WindowObject.SurfaceWithContent
                          && !winChrome.manuallyClosed
                    PropertyChanges {
                        winChrome {
                            opacity: 1
                            scale: 1
                            visible: true
                        }
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
                            if (winChrome.window.contentState === WindowObject.NoSurface
                                    || winChrome.manuallyClosed)
                                topLevelWindowsModel.remove(winChrome.index, 1)
                        } }
                    }
                }
            ]
        }
    }

    // Handler for WindowManager signals
    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            topLevelWindowsModel.append({"window":window})
        }
    }
}
