// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtApplicationManager.SystemUI

Rectangle {
    width: 1024
    height: 640
    color: "linen"

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
            required property WindowObject window
            required property int index

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
            }

            Component.onCompleted: {
                winChrome.x =  300 + winChrome.index * 50;
                winChrome.y =  10 + winChrome.index * 30;
            }

            // Its default state represents a closed window. It's the starting point for
            // an open animation and the end point for a close animation.
            opacity: 0.25
            scale: 0.50
            visible: false

            // Animation code from here on
            states: [
                State {
                    name: "open"
                    when: winChrome.window && winChrome.window.contentState === WindowObject.SurfaceWithContent
                    PropertyChanges {
                        winChrome {
                            opacity: 1
                            scale: 1
                            visible: true
                        }
                    }
                }
            ]

            // true when the window is not being shown on the screen anymore.
            //
            // When the state changes away from "open", the visible property momentarily turns to false
            // before being put back to true again for the disappearance animation. Hence the need for
            // a more stable property.
            property bool fullyDisappeared: true

            transitions: [
                Transition { // window appears
                    to: "open"
                    PropertyAction { target: winChrome; property: "fullyDisappeared"; value: false }
                    NumberAnimation { target: winChrome; properties: "opacity,scale"; duration: 500; easing.type: Easing.OutQuad}
                },
                Transition { // window disappears
                    id: disappearTransition
                    from: "open"
                    SequentialAnimation {
                        PropertyAction { target: winChrome; property: "visible"; value: true } // we wanna see the window during the closing animation
                        NumberAnimation { target: winChrome; properties: "opacity,scale"; duration: 500; easing.type: Easing.InQuad}
                        PropertyAction { target: winChrome; property: "fullyDisappeared"; value: true }
                    }
                }
            ]

            readonly property bool safeToRemove: fullyDisappeared && winChrome.window && winChrome.window.contentState === WindowObject.NoSurface
            onSafeToRemoveChanged: if (safeToRemove) windowsModel.remove(winChrome.index, 1)
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
