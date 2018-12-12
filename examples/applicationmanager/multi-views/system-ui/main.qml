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
import QtApplicationManager.SystemUI 2.0

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

                Text {
                    anchors.fill: parent
                    fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: height
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: model.isRunning ? "Stop" : "Start"
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
                text: (windowItem.primary ? "Primary: " : "Secondary: ") + model.window.application.name("en")
            }

            MouseArea {
                anchors.fill: parent
                drag.target: parent
                onPressed: topLevelWindowsModel.move(model.index, topLevelWindowsModel.count-1, 1);
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
                        winChrome.manuallyClosed = true;
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
                        topLevelWindowsModel.append({"window":model.window});
                    }
                }
            }

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
                            if (model.window.contentState === WindowObject.NoSurface || winChrome.manuallyClosed)
                                topLevelWindowsModel.remove(model.index, 1);
                        } }
                    }
                }
            ]
        }
    }

    // Handler for WindowManager signals
    Connections {
        target: WindowManager
        onWindowAdded:  {
            topLevelWindowsModel.append({"window":window});
        }
    }
}
