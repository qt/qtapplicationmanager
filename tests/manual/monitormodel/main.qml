// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11

import QtApplicationManager 2.0

Window {
    width: 1200
    height: 700

    Pane {
        anchors.fill: parent

        MonitorModel {
            id: monitorModel

            running: runningSwitch.checked

            // Data sources from QtApplicationManager
            CpuStatus { id: cpuStatus }
            MemoryStatus { id: memoryStatus }
            IoStatus {
                id: ioStatus
                deviceNames: "sda"
            }

            // A custom data source, with two roles
            QtObject {
                id: fooBarStatus
                property var roleNames: ["foo", "bar"]

                function update() {
                    foo += 1;

                    if (_up) {
                        bar += 1;
                        if (bar == 10)
                            _up = false;
                    } else {
                        bar -= 1;
                        if (bar == 0)
                            _up = true;
                    }
                }
                property int foo: 0
                property int bar: 10
                property bool _up: false
            }

            // Yet another custom data source
            QtObject {
                id: alphaStatus
                property var roleNames: ["alpha"]
                function update() {
                    if (_up) {
                        alpha *= 2;
                        if (alpha == 1024)
                            _up = false;
                    } else {
                        alpha /= 2;
                        if (alpha == 2)
                            _up = true;
                    }
                }
                property real alpha: 2
                property bool _up: true
            }
        }

        ScrollView {
            id: controls
            clip: true
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            ColumnLayout {
                PropertyField { name: "maximumCount"; object: monitorModel }
                PropertyField { name: "interval"; object: monitorModel }

                RowLayout {
                    ComboBox {
                        id: rolesCombo
                        Layout.fillWidth: true
                        model: [ "cpuLoad", "memoryUsed", "foo", "bar", "alpha" ]

                    }
                    Button {
                        text: "Add"
                        onClicked: {
                            chartsModel.append({"role" : rolesCombo.currentText });
                        }
                    }
                }

                Switch {
                    id: runningSwitch
                    text: "running"
                    checked: false
                }

                Button { text: "Clear"; onClicked: monitorModel.clear() }

                Label { text: "cpuLoad: " + cpuStatus.cpuLoad }
                Label { text: "totalMemory: " + memoryStatus.totalMemory }
                Label { text: "memoryUsed: " + memoryStatus.memoryUsed }
                Label { text: "ioLoad.sda: " + ioStatus.ioLoad.sda }
                Label { text: "foo: " + fooBarStatus.foo }
                Label { text: "bar: " + fooBarStatus.bar }
                Label { text: "alpha: " + alphaStatus.alpha }
            }
        }

        ColumnLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: controls.right
            anchors.right: parent.right

            Repeater {
                model: ListModel { id: chartsModel }
                delegate: SystemMonitorChart {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceModel: monitorModel
                    role: model.role
                    onRemoveClicked: chartsModel.remove(model.index)
                }
            }
        }
    }
}
