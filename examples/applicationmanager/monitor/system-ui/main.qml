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

import QtQuick 2.6
import QtQuick.Window 2.0
import QtApplicationManager 1.0


Window {
    id: root

    property var primaryWindow
    property var secondaryWindow

    width: 720
    height: 600
    color: "black"

    Column {
        x: 10
        padding: 20

        Tile {
            Column {
                id: systemOverview
                spacing: 10

                MonitorText { text: "System"; font.pixelSize: 26 }
                MonitorText { text: "CPU Cores: " + SystemMonitor.cpuCores }
                MonitorText { text: "Total Memory: " + (SystemMonitor.totalMemory / 1e9).toFixed(1) + " GB" }
                MonitorText { id: systemMem; text: "Used Memory:"  }
                MonitorText { text: "Idle Threshold: " + SystemMonitor.idleLoadThreshold * 100 + " %" }
                MonitorText { text: "Idle: " + SystemMonitor.idle }
                MonitorText { text: "GPU Load: " + SystemMonitor.gpuLoad * 100 + " %" }
            }
        }

        Tile {
            MonitorChart {
                id: systemLoad
                title: "CPU Load"
                model: SystemMonitor
                delegate: Rectangle {
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
        }
    }

    Rectangle {
        x: 246; y: 25
        width: 1; height: root.height - 50
        color: "white"
    }

    Grid {
        columns: 2
        padding: 20
        x: 260

        Tile {
            Column {
                spacing: 20

                MonitorText {
                    text: processMon.applicationId === "" ? "System-UI" : processMon.applicationId
                    font.pixelSize: 26
                }
                MonitorText { text: "Process ID: " + processMon.processId }

                Switch {
                    onToggle: {
                        processMon.applicationId = processMon.applicationId === ""
                                                         ? ApplicationManager.application(0).id : ""
                        if (processMon.applicationId !== "") {
                            if (ApplicationManager.singleProcess)
                                console.warn("There is no dedicated application process in single-process mode");

                            if (primaryWindow && secondaryWindow) {
                                processMon.monitoredWindows = primary.active ? [primaryWindow] : [secondaryWindow]
                            } else {
                                processMon.monitoredWindows = []
                                console.warn("No application window available. Please check your QtWayland configuration.");
                            }
                        } else {
                            processMon.monitoredWindows = [root]
                        }
                    }
                }
            }
        }

        Tile {
            Column {
                visible: processMon.applicationId !== ""
                spacing: 20
                Item {
                    width: 200; height: 65
                    MonitorText { anchors.bottom: parent.bottom; text: "Application Windows:" }
                }

                WindowContainer {
                    id: primary
                    active: true
                    onActivated: {
                        processMon.monitoredWindows = [primaryWindow];
                        secondary.active = false;
                    }
                }

                WindowContainer {
                    id: secondary
                    onActivated: {
                        processMon.monitoredWindows = [secondaryWindow];
                        primary.active = false;
                    }
                }
            }
        }

        Tile {
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
        }

        Tile {
            MonitorChart {
                id: frameChart
                title: "Frame rate"
                model: processMon
                delegate: Rectangle {
                    width: 11
                    height: frameRate[0] ? frameRate[0].average * 3 : 0
                    anchors.bottom: parent.bottom
                    color.r: frameRate[0] ? (frameRate[0].average >= 60 ? 0 : 1 - frameRate[0].average / 60) : 0
                    color.g: frameRate[0] ? (frameRate[0].average >= 60 ? 1 : frameRate[0].average / 60) : 0
                    color.b: 0

                }
            }
        }
    }

    Connections {
        target: WindowManager
        onWindowReady:  {
            if (WindowManager.windowProperty(window, "windowType") === "primary") {
                primaryWindow = window
                window.parent = primary.container
                window.anchors.fill = primary.container
            } else {
                secondaryWindow = window
                window.parent = secondary.container
                window.anchors.fill = secondary.container
            }
        }

        onWindowLost: {
            processMon.monitoredWindows = []
            WindowManager.releaseWindow(window);
        }
    }

    ProcessMonitor {
        id: processMon
        applicationId: ""
        reportingInterval: 1000
        count: 12

        memoryReportingEnabled: true
        frameRateReportingEnabled: true
        monitoredWindows: [root]

        onMemoryReportingChanged: processPss.reading = (memoryPss.total / 1e6).toFixed(0) + " MB";
        onFrameRateReportingChanged: {
            if (frameRate[0])
                frameChart.reading = frameRate[0].average.toFixed(0) + " fps";
        }
    }

    Connections {
        target: SystemMonitor
        onCpuLoadReportingChanged: systemLoad.reading = (load * 100).toFixed(1) + " %";
        onMemoryReportingChanged: systemMem.text = "Used Memory: " + (used / 1e9).toFixed(1) + " GB"
    }


    Component.onCompleted: {
        SystemMonitor.reportingInterval = 1000;
        SystemMonitor.count = 12;

        SystemMonitor.idleLoadThreshold = 0.05;
        SystemMonitor.cpuLoadReportingEnabled = true;
        SystemMonitor.gpuLoadReportingEnabled = true;
        SystemMonitor.memoryReportingEnabled = true;

        ApplicationManager.startApplication(ApplicationManager.application(0).id);
    }
}
