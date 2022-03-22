/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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
****************************************************************************/

import QtQuick 2.4
import QtQuick.Layouts 1.11
import QtApplicationManager.SystemUI 2.0

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
                text: model.window.application.names["en"] + " application window"
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
            }

            Component.onCompleted: {
                winChrome.x =  300 + model.index * 50;
                winChrome.y =  10 + model.index * 30;
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
                    when: model.window && model.window.contentState === WindowObject.SurfaceWithContent
                    PropertyChanges {
                        target:  winChrome
                        opacity: 1
                        scale: 1
                        visible: true
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

            readonly property bool safeToRemove: fullyDisappeared && model.window && model.window.contentState === WindowObject.NoSurface
            onSafeToRemoveChanged: if (safeToRemove) windowsModel.remove(model.index, 1)
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
