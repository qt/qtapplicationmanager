/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11

import QtApplicationManager.SystemUI 1.0

Window {
    width: 1200
    height: 700

    Pane {
        anchors.fill: parent

        SystemMonitor {
            id: systemMonitor
            cpuLoadReportingEnabled: cpuLoadReportingSwitch.checked
            memoryReportingEnabled: memoryReportingSwitch.checked
            gpuLoadReportingEnabled: gpuLoadReportingSwitch.checked
            fpsReportingEnabled: fpsReportingSwitch.checked
        }

        ScrollView {
            id: controls
            clip: true
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            ColumnLayout {
                PropertyField { name: "count"; object: systemMonitor }
                PropertyField { name: "reportingInterval"; object: systemMonitor }
                PropertyField { name: "idleLoadThreshold"; object: systemMonitor }

                RowLayout {
                    ComboBox {
                        id: rolesCombo
                        // TODO: ioLoad
                        Layout.fillWidth: true
                        model: [ "cpuLoad", "gpuLoad", "memoryUsed", "averageFps", "minimumFps",
                                 "maximumFps", "fpsJitter" ]

                    }
                    Button {
                        text: "Add"
                        onClicked: {
                            chartsModel.append({"role" : rolesCombo.currentText });
                        }
                    }
                }

                Switch {
                    id: cpuLoadReportingSwitch
                    text: "cpuLoadReportingEnabled"
                    checked: false
                }

                Switch {
                    id: memoryReportingSwitch
                    text: "memoryReportingEnabled"
                    checked: false
                }

                Switch {
                    id: gpuLoadReportingSwitch
                    text: "gpuLoadReportingEnabled"
                    checked: false
                }

                Switch {
                    id: fpsReportingSwitch
                    text: "fpsReportingEnabled"
                    checked: false
                }

                Label { text: "cpuLoad: " + systemMonitor.cpuLoad }
                Label { text: "totalMemory: " + systemMonitor.totalMemory }
                Label { text: "memoryUsed: " + systemMonitor.memoryUsed }
                Label { text: "cpuCores: " + systemMonitor.cpuCores }
                Label { text: "cpuLoad: " + systemMonitor.cpuLoad }
                Label { text: "gpuLoad: " + systemMonitor.gpuLoad }
                Label { text: "idle: " + systemMonitor.idle }
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
                    sourceModel: systemMonitor
                    role: model.role
                    onRemoveClicked: chartsModel.remove(model.index)
                }
            }
        }
    }
}
