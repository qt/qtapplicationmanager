// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import QtApplicationManager

Window {
    id: root
    width: 700
    height: 600
    title: "PSI Monitor"

    SystemStatus {
        id: systemStatus

        // Please note: unprivileged user can only set timeWindows in multiple of 2000ms

        cpuPSI.stallTime: 100
        cpuPSI.timeWindow: 2000
        memoryPSI.stallTime: 100
        memoryPSI.timeWindow: 2000
        ioPSI.stallTime: 100
        ioPSI.timeWindow: 2000
    }

    CGroupStatus {
        id: cgroupStatus

        cpuPSI.stallTime: 100
        cpuPSI.timeWindow: 2000
        memoryPSI.stallTime: 100
        memoryPSI.timeWindow: 2000
        ioPSI.stallTime: 100
        ioPSI.timeWindow: 2000
    }

    function appendLog(tag, source) {
        var ts = new Date().toLocaleTimeString(Qt.locale(), "HH:mm:ss.zzz")
        logArea.text += "[" + ts + "] [" + tag + "] " + source + " PSI triggered\n"
        logArea.cursorPosition = logArea.length
    }

    component PsiPage: Pane {
        id: page

        required property var statusObject
        required property string subtitle
        required property string tag

        Connections {
            target: page.statusObject.cpuPSI
            function onTriggered() { root.appendLog(page.tag, "CPU") }
        }
        Connections {
            target: page.statusObject.memoryPSI
            function onTriggered() { root.appendLog(page.tag, "Memory") }
        }
        Connections {
            target: page.statusObject.ioPSI
            function onTriggered() { root.appendLog(page.tag, "I/O") }
        }

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            spacing: 12

            Label {
                text: page.subtitle
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: [
                        { label: "CPU",    psi: page.statusObject.cpuPSI    },
                        { label: "Memory", psi: page.statusObject.memoryPSI },
                        { label: "I/O",    psi: page.statusObject.ioPSI     }
                    ]

                    GroupBox {
                        id: psiBox
                        required property var modelData
                        title: psiBox.modelData.label
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Off", "Some", "Full"]
                                currentIndex: psiBox.modelData.psi.mode
                                onActivated: psiBox.modelData.psi.mode = currentIndex
                            }

                            GridLayout {
                                columns: 2

                                Label { text: "Window (ms):" }
                                SpinBox {
                                    Layout.fillWidth: true
                                    editable: true
                                    from: 0
                                    to: 10000
                                    stepSize: 2000
                                    value: psiBox.modelData.psi.timeWindow
                                    onValueModified: psiBox.modelData.psi.timeWindow = value
                                }

                                Label { text: "Stall (ms):" }
                                SpinBox {
                                    Layout.fillWidth: true
                                    editable: true
                                    from: 0
                                    to: 10000
                                    stepSize: 100
                                    value: psiBox.modelData.psi.stallTime
                                    onValueModified: psiBox.modelData.psi.stallTime = value
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton { text: "System" }
            TabButton { text: "CGroup" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.maximumHeight: implicitHeight
            currentIndex: tabBar.currentIndex

            PsiPage {
                statusObject: systemStatus
                subtitle: "System-wide PSI"
                tag: "System"
            }

            PsiPage {
                statusObject: cgroupStatus
                subtitle: "CGroup: " + cgroupStatus.path
                tag: "CGroup"
            }
        }

        Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        id: logArea
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        font.family: "monospace"
                        placeholderText: "PSI trigger events will appear here..."
                    }
                }

                Button {
                    text: "Clear log"
                    onClicked: logArea.text = ""
                }
            }
        }
    }
}
