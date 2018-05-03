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

import QtQuick 2.8
import QtQuick.Window 2.2
import QtApplicationManager 1.0
import QtApplicationManager 1.0 as AppMan

Window {
    id: root
    width: 1024
    height: 640

    Loader {
        anchors.fill: parent
        source: "../test.qml"
    }

    Rectangle {
        width: 10
        height: 10
        color: "red"
        NumberAnimation on rotation {
            loops: Animation.Infinite
            from: 0
            to: 360
        }
    }

    property int loadedApps: 0

    Grid {
        id: grid
        anchors.fill: parent

        columns: Math.ceil(Math.sqrt(ApplicationManager.count))

        Repeater {
            id: windows
            model: ApplicationManager.count

            Item {
                id: winChrome

                property alias appContainer: appContainer
                property string appId
                property alias processMonitor: appProcessMonitor

                width: root.width / grid.columns;
                height: root.height / Math.ceil(ApplicationManager.count / grid.columns)
                visible: false

                Item {
                    id: appContainer
                    anchors.fill: parent
                    anchors.margins: 3
                    anchors.topMargin: 25
                }

                ProcessMonitor {
                    id: appProcessMonitor
                    applicationId: winChrome.appId
                    reportingInterval: 100
                    count: 100
                    memoryReportingEnabled: true
                    cpuLoadReportingEnabled: true
                    frameRateReportingEnabled: true

                    property int cpuReportings
                    property int memoryReportings
                    property int fpsReportings

                    onCpuLoadReportingChanged: { cpuReportings++ }
                    onMemoryReportingChanged: { memoryReportings++ }
                    onFrameRateReportingChanged: { fpsReportings++ }
                }
            }
        }
    }

    Connections {
        target: WindowManager
        onWindowReady:  {
            var appId = WindowManager.get(index).applicationId;
            var appIndex = ApplicationManager.indexOfApplication(appId);
            console.log("SystemUI: onWindowReady [" + window + "] - index: "
                        + index + ", appIndex: " + appIndex );

            var chrome = windows.itemAt(appIndex);
            window.parent = chrome.appContainer;
            window.anchors.fill = chrome.appContainer;
            chrome.visible = true;
            chrome.appId = appId
            chrome.processMonitor.monitoredWindows = [ window ]

            if (WindowManager.count >= ApplicationManager.count)
                statsTimer.start()
        }

        onWindowClosing: {
            console.log("SystemUI: onWindowClosing [" + window + "] - index: " + index);
            var appIndex = ApplicationManager.indexOfApplication(WindowManager.get(index).applicationId);
            if (appIndex === -1)
                appIndex = 0;
            windows.itemAt(appIndex).visible = false;
        }

        onWindowLost: {
            console.log("SystemUI: onWindowLost [" + window + "] - index: " + index);
            WindowManager.releaseWindow(window);
        }
    }

    LoggingCategory {
        id: benchCategory
        name: "am.bench"
    }

    ProcessMonitor {
        id: systemUiMonitor
        applicationId: ""
        count: 100
        reportingInterval: 100
        cpuLoadReportingEnabled: true
        memoryReportingEnabled: true
        frameRateReportingEnabled: true

        property int cpuReportings
        property int memoryReportings
        property int fpsReportings

        onCpuLoadReportingChanged: { cpuReportings++ }
        onMemoryReportingChanged: { memoryReportings++ }
        onFrameRateReportingChanged: { fpsReportings++ }
    }

    Timer {
        id: statsTimer
        repeat: false
        interval: 2000
        onTriggered: {
            var load = avg(systemUiMonitor, systemUiMonitor.cpuReportings, "cpuLoad")
            var pss = "System-UI PSS Max: " + format((max(systemUiMonitor, systemUiMonitor.memoryReportings, "memoryPss.total") / 1e6).toFixed(0)) + " MB";
            var rss = "System-UI RSS Max: " + format((max(systemUiMonitor, systemUiMonitor.memoryReportings, "memoryRss.total") / 1e6).toFixed(0)) + " MB";
            var fps = "System-UI FPS Avg: " + format((avg_fps(systemUiMonitor)).toFixed(2)) + " fps";
            var cpuLoad = "System-UI CPU Load Avg: " + format((load * 100).toFixed(2)) + " %";
            var gpuLoad = "System GPU Load Avg: " + format((avg(SystemMonitor, SystemMonitor.count, "gpuLoad") * 100).toFixed(0)) + " %";

            var totalAppPSS = 0
            var totalAppRSS = 0
            var totalAppFPS = 0
            var totalCPULoad = load
            for (var i=0; i<ApplicationManager.count; i++) {
                var chrome = windows.itemAt(i);
                totalAppRSS += max(chrome.processMonitor, chrome.processMonitor.memoryReportings, "memoryRss.total")
                totalAppPSS += max(chrome.processMonitor, chrome.processMonitor.memoryReportings, "memoryPss.total")
                totalAppFPS += avg_fps(chrome.processMonitor)
                totalCPULoad += avg(chrome.processMonitor, chrome.processMonitor.cpuReportings, "cpuLoad")
                ApplicationManager.stopApplication(ApplicationManager.application(i).id);
            }

            var appRSS = "App RSS Max: " + format((totalAppRSS / ApplicationManager.count / 1e6).toFixed(0)) + " MB";
            var appPSS = "App PSS Max: " + format((totalAppPSS / ApplicationManager.count / 1e6).toFixed(0)) + " MB";
            var appFPS = "App FPS Avg: " + format((totalAppFPS / ApplicationManager.count ).toFixed(0)) + " fps";

            var accCpuLoad = "Acc. CPU Load Avg: " + format((totalCPULoad * 100).toFixed(2)) + " %";

            console.log(benchCategory, "System-UI Resolution: " + root.width + "x" + root.height + " App Resolution: " +  windows.itemAt(0).width + "x" + windows.itemAt(0).height)
            console.log(benchCategory, cpuLoad + " " + accCpuLoad + " " + gpuLoad + " " + fps + " " + rss + " " + pss + " " + appRSS + " " + appPSS + " " + appFPS)
            Qt.quit();
        }
    }

    function max(monitor, size, key) {
        var value = 0
        for (var i=0; i<size; i++) {
            var val = eval("monitor.get(i)." + key)
            if (val == undefined)
                continue
            value = Math.max(val, value)
        }
        return value
    }

    function avg(monitor, size, key) {
        var sum = 0
        for (var i=0; i< size; i++) {
            var val = eval("monitor.get(i)." + key)
            if (val == undefined)
                break
            sum += val
        }
        return sum / size
    }

    function avg_fps(monitor) {
        var sum = 0
        var counter = 0
        for (var i=0; i< monitor.fpsReportings; i++) {
            var val = monitor.get(i).frameRate[0]
            if (val == undefined)
                continue
            sum += val.average
            counter++
        }
        return sum / counter
    }

    function format(n) {
        return String("      " + n).slice(-6);
    }

    Component.onCompleted: {
        for (var i=0; i<ApplicationManager.count; i++) {
            ApplicationManager.startApplication(ApplicationManager.application(i).id);
        }

        SystemMonitor.reportingInterval = 100;
        SystemMonitor.count = 100;

        SystemMonitor.idleLoadThreshold = 0.05;
        SystemMonitor.gpuLoadReportingEnabled = true;

        systemUiMonitor.monitoredWindows = [root]
    }
}
