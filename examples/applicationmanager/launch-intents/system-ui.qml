// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager
import QtApplicationManager.SystemUI

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
                id: delegate
                required property url icon
                required property string applicationId
                required property string intentId
                required property string name
                Image {
                    source: delegate.icon
                    MouseArea {
                        anchors.fill: parent
                        onPressAndHold: {
                            var app = ApplicationManager.application(delegate.applicationId)
                            if (app.runState === Am.Running)
                                app.stop()
                        }
                        onClicked: {
                            IntentClient.sendIntentRequest(delegate.intentId,
                                                           delegate.applicationId, {})
                        }
                    }
                }
                Text {
                    font.pixelSize: 20
                    text: delegate.name
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
                required property var model
                width: 600
                height: 200
                window: model.window
            }
        }
    }
}
