// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.12
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0

Item {
    width: 800
    height: 600

    IntentModel {
        id: intentModel
        filterFunction: function(i) { return i.categories.includes("launcher") }
        sortFunction: function(li, ri) { return li.name > ri.name }
    }

    // Show intent names and icons
    Column {
        spacing: 20
        Repeater {
            model: intentModel
            Column {
                Image {
                    source: model.icon
                    MouseArea {
                        anchors.fill: parent
                        onPressAndHold: {
                            var app = ApplicationManager.application(model.applicationId)
                            if (app.runState === Am.Running)
                                app.stop()
                        }
                        onClicked: {
                            IntentClient.sendIntentRequest(model.intentId, model.applicationId, {})
                        }
                    }
                }
                Text {
                    font.pixelSize: 20
                    text: model.name
                }
            }
        }
    }

    // Show windows
    Column {
        anchors.right: parent.right
        Repeater {
            model: WindowManager
            WindowItem {
                width: 600
                height: 200
                window: model.window
            }
        }
    }
}
