/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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
import QtQuick.Window 2.0
import QtApplicationManager 1.0

Window {
    width: 652
    height: 592
    color: "black"

    Column {
        x: 30; y: 30
        spacing: 30

        Row {
            spacing: 30

            Column {
                id: systemOverview
                spacing: 10

                MonitorText { text: "System"; font.pixelSize: 26 }
                MonitorText { text: "CPU Cores: " + SystemMonitor.cpuCores }
                MonitorText { text: "Total Memory: " + (SystemMonitor.totalMemory / 1e9).toFixed(1) + " GB" }
                MonitorText { id: systemMem; text: "Used Memory:"  }
                MonitorText { text: "Idle Threshold: " + SystemMonitor.idleLoadThreshold * 100 + " %" }
                MonitorText { text: "Idle: " + SystemMonitor.idle }

                Spin {}
            }

            MonitorChart {
                id: systemFps
                title: "Frame Rate"
                model: SystemMonitor
                delegate: Rectangle {
                    id: fpsdel
                    width: 11
                    height: averageFps * 3
                    anchors.bottom: parent.bottom
                    color.r: averageFps >= 60 ? 0 : 1 - averageFps / 60
                    color.g: averageFps >= 60 ? 1 : averageFps / 60
                    color.b: 0
                }
            }

            MonitorChart {
                id: systemLoad
                title: "CPU Load"
                model: SystemMonitor
                delegate: cpuDelegate
            }
        }

        Row {
            spacing: 30

            Column {
                spacing: 10

                MonitorText { text: "Process"; font.pixelSize: 26 }
                MonitorText { text: processMon.applicationId === "" ? "System-UI" : processMon.applicationId }
                MonitorText { text: "Process ID: " + processMon.processId }

                Switch {
                    width: systemOverview.width
                    onToggle: processMon.applicationId = processMon.applicationId === ""
                                                         ? ApplicationManager.application(0).id : ""
                }
            }

            MonitorChart {
                id: processPss
                title: "Memory PSS"
                model: processMon
                delegate: Rectangle {
                    width: 11
                    height: memoryPss.total / 5e6
                    anchors.bottom: parent.bottom
                    color: "lightsteelblue"
                }
            }

            MonitorChart {
                id: processLoad
                title: "CPU Usage"
                model: processMon
                delegate: cpuDelegate
            }
        }
    }

    Component {
        id: cpuDelegate
        Rectangle {
            width: 11
            height: parent.height
            gradient: Gradient {
                GradientStop { position: 0.5; color: "#FF0000" }
                GradientStop { position: 1.0; color: "#00FF00" }
            }
            Rectangle {
                width: parent.width
                height: parent.height - cpuLoad * parent.height
                color: "black"
            }
        }
    }

    ProcessMonitor {
        id: processMon
        applicationId: ""
        reportingInterval: 1000
        count: 12

        memoryReportingEnabled: true
        cpuLoadReportingEnabled: true

        onMemoryReportingChanged: processPss.reading = (memoryPss.total / 1e6).toFixed(0) + " MB";
        onCpuLoadReportingChanged: processLoad.reading = (load * 100).toFixed(1) + " %";
    }

    Connections {
        target: SystemMonitor
        onCpuLoadReportingChanged: systemLoad.reading = (load * 100).toFixed(1) + " %";
        onMemoryReportingChanged: systemMem.text = "Used Memory: " + (used / 1e9).toFixed(1) + " GB"
        onFpsReportingChanged: systemFps.reading = average.toFixed(1) + " fps";
    }

    Component.onCompleted: {
        SystemMonitor.reportingInterval = 1000;
        SystemMonitor.count = 12;

        SystemMonitor.idleLoadThreshold = 0.05;
        SystemMonitor.cpuLoadReportingEnabled = true;
        SystemMonitor.fpsReportingEnabled = true;
        SystemMonitor.memoryReportingEnabled = true;

        ApplicationManager.startApplication(ApplicationManager.application(0).id);
    }
}
