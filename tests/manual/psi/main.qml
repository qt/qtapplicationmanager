// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import QtApplicationManager

Window {
    width: 700
    height: 600
    title: "PSI Monitor - " + cgroupStatus.path

    CGroupStatus {
        id: cgroupStatus

        // Please note: unprivileged user can only set timeWindows in multiple of 2000ms

        cpuPSI.stallTime: 100
        cpuPSI.timeWindow: 2000
        memoryPSI.stallTime: 100
        memoryPSI.timeWindow: 2000
        ioPSI.stallTime: 100
        ioPSI.timeWindow: 2000
    }

    Connections {
        target: cgroupStatus.cpuPSI
        function onTriggered() { appendLog("CPU") }
    }
    Connections {
        target: cgroupStatus.memoryPSI
        function onTriggered() { appendLog("Memory") }
    }
    Connections {
        target: cgroupStatus.ioPSI
        function onTriggered() { appendLog("I/O") }
    }

    function appendLog(source) {
        var ts = new Date().toLocaleTimeString(Qt.locale(), "HH:mm:ss.zzz")
        logArea.text += "[" + ts + "] " + source + " PSI triggered\n"
        logArea.cursorPosition = logArea.length
    }

    Pane {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            RowLayout {
                spacing: 12

                Repeater {
                    model: [
                        { label: "CPU",    psi: cgroupStatus.cpuPSI    },
                        { label: "Memory", psi: cgroupStatus.memoryPSI },
                        { label: "I/O",    psi: cgroupStatus.ioPSI     }
                    ]

                    GroupBox {
                        title: modelData.label
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Off", "Some", "Full"]
                                currentIndex: modelData.psi.mode
                                onActivated: modelData.psi.mode = currentIndex
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
                                    value: modelData.psi.timeWindow
                                    onValueModified: modelData.psi.timeWindow = value
                                }

                                Label { text: "Stall (ms):" }
                                SpinBox {
                                    Layout.fillWidth: true
                                    editable: true
                                    from: 0
                                    to: 10000
                                    stepSize: 100
                                    value: modelData.psi.stallTime
                                    onValueModified: modelData.psi.stallTime = value
                                }
                            }
                        }
                    }
                }
            }

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
