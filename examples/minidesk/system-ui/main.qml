/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

import QtQuick 2.4
import QtApplicationManager 1.0

Rectangle {
    property int zorder: 1

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

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        ApplicationManager.startApplication(applicationId, "documentUrl");
                        parent.opacity = 0.3;
                    }
                }
            }
        }
    }

    // System UI chrome for applications
    Repeater {
        id: windows
        model: menuItems.model

        Rectangle {
            id: winChrome

            property alias appContainer: appContainer

            width: 400; height: 320
            x: 300 + model.index * 50; y: 10 + model.index * 30
            color: "tan"
            visible: false

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Decoration: " + name
            }

            MouseArea {
                anchors.fill: parent
                drag.target: parent
                onPressed: parent.z = zorder++;   // for demo purposes only
            }

            Rectangle {
                width: 25; height: 25
                color: "chocolate"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        winChrome.visible = false;
                        menuItems.itemAt(model.index).opacity = 1.0;
                        ApplicationManager.stopApplication(applicationId, false);
                    }
                }
            }

            Item {
                id: appContainer
                anchors.fill: parent
                anchors.margins: 3
                anchors.topMargin: 25
            }
        }
    }

    // System UI for a pop-up and notification
    Item {
        id: popUpContainer
        z: 30000
        width: 200; height: 60
        anchors.centerIn: parent
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
        onWindowReady:  {
            var appIndex = ApplicationManager.indexOfApplication(WindowManager.get(index).applicationId);
            var type = WindowManager.windowProperty(window, "type");
            console.log("SystemUI: onWindowReady [" + window + "] - index: "
                         + index + ", appIndex: " + appIndex + ", type: " + type);

            if (type !== "pop-up") {
                if (appIndex === -1) {
                    console.log("Allowing a single app started outside of appman instead of App1 ...");
                    appIndex = 0;
                }
                var chrome = windows.itemAt(appIndex);
                window.parent = chrome.appContainer;
                window.anchors.fill = chrome.appContainer;
                chrome.visible = true;
                WindowManager.setWindowProperty(window, "propA", 42)
            } else {
                window.parent = popUpContainer;
                window.anchors.fill = popUpContainer;
            }
        }

        onWindowPropertyChanged: console.log("SystemUI: OnWindowPropertyChanged [" + window + "] - "
                                               + name + ": " + value);

        onWindowClosing: console.log("SystemUI: onWindowClosing [" + window + "] - index: " + index);

        onWindowLost: {
            console.log("SystemUI: onWindowLost [" + window + "] - index: " + index);
            WindowManager.releaseWindow(window);
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
