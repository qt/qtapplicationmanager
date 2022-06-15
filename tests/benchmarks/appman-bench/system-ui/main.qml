// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.8
import QtQuick.Window 2.2
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0

Window {
    id: root
    width: 1024
    height: 640

    LoggingCategory {
        id: benchCategory
        name: "am.bench"
    }

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

    Grid {
        id: grid
        anchors.fill: parent

        columns: Math.ceil(Math.sqrt(ApplicationManager.count))

        Repeater {
            id: windows
            model: WindowManager

            Item {
                property alias processMonitor: appProcessMonitor
                property alias frameTimer: appFrameTimer

                width: root.width / grid.columns;
                height: root.height / Math.ceil(ApplicationManager.count / grid.columns)

                WindowItem {
                    anchors.fill: parent
                    anchors.margins: 3
                    anchors.topMargin: 25
                    window: model.window
                }

                MonitorModel {
                    id: appProcessMonitor
                    maximumCount: 100
                    running: true
                    interval: 100

                    ProcessStatus {
                        applicationId: model.window.application.id
                    }

                    FrameTimer {
                        id: appFrameTimer
                        window: model.window
                    }
                }
            }
        }
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            console.log("System UI: onWindowAdded: " + window);
            if (WindowManager.count >= ApplicationManager.count)
                statsTimer.start();
        }
    }

    MonitorModel {
        id: systemUiMonitor
        maximumCount: 100
        running: true
        interval: 100

        ProcessStatus {
            applicationId: ""
        }

        FrameTimer {
            id: systemFrameTimer
            window: root
        }

        GpuStatus {}
    }

    Timer {
        id: statsTimer
        interval: 2000
        onTriggered: {
            systemUiMonitor.running = false;
            var load = avg(systemUiMonitor, systemUiMonitor.count, "cpuLoad");
            var rss = "System UI RSS Max: " + format((max(systemUiMonitor, systemUiMonitor.count, "memoryRss.total")
                                                      / 1e6).toFixed(0)) + " MB";
            var pss = "System UI PSS Max: " + format((max(systemUiMonitor, systemUiMonitor.count, "memoryPss.total")
                                                      / 1e6).toFixed(0)) + " MB";
            var fps = "System UI FPS Avg: " + format(systemFrameTimer.averageFps.toFixed(1)) + " fps";
            var cpuLoad = "System UI CPU Load Avg: " + format((load * 100).toFixed(1)) + " %";
            var gpuLoad = "System GPU Load Avg: " + format((avg(systemUiMonitor, systemUiMonitor.count, "gpuLoad")
                                                            * 100).toFixed(0)) + " %";

            var totalAppRSS = 0;
            var totalAppPSS = 0;
            var totalAppFPS = 0;
            var totalCPULoad = load;
            for (var i = 0; i < windows.count; i++) {
                var chrome = windows.itemAt(i);
                chrome.processMonitor.running = false;
                totalAppRSS += max(chrome.processMonitor, chrome.processMonitor.count, "memoryRss.total");
                totalAppPSS += max(chrome.processMonitor, chrome.processMonitor.count, "memoryPss.total");
                totalAppFPS += chrome.processMonitor.get(chrome.processMonitor.count - 1).averageFps ;
                totalCPULoad += avg(chrome.processMonitor, chrome.processMonitor.count, "cpuLoad");
                ApplicationManager.stopApplication(ApplicationManager.application(i).id);
            }

            var appRSS = "App RSS Max: " + format((totalAppRSS / ApplicationManager.count / 1e6).toFixed(0)) + " MB";
            var appPSS = "App PSS Max: " + format((totalAppPSS / ApplicationManager.count / 1e6).toFixed(0)) + " MB";
            var appFPS = "App FPS Avg: " + format((totalAppFPS / ApplicationManager.count ).toFixed(1)) + " fps";

            var accCpuLoad = "Acc. CPU Load Avg: " + format((totalCPULoad * 100).toFixed(1)) + " %";

            console.log(benchCategory, "System UI Resolution: " + root.width + "x" + root.height + " | App Resolution: "
                                         +  windows.itemAt(0).width + "x" + windows.itemAt(0).height);
            console.log(benchCategory, cpuLoad + " | " + accCpuLoad + " | " + gpuLoad + " | " + fps + " | " + rss
                                         + " | " + pss + " | " + appFPS + " | " + appRSS + " | " + appPSS);
            Qt.quit();
        }
    }

    function max(monitor, size, key) {
        var value = 0;
        for (var i = 0; i < size; i++) {
            var val = eval("monitor.get(i)." + key);
            if (val)
                value = Math.max(val, value);
        }
        return value;
    }

    function avg(monitor, size, key) {
        var sum = 0;
        for (var i = 0; i < size; i++) {
            var val = eval("monitor.get(i)." + key)
            if (val == undefined)
                break;
            sum += val;
        }
        return sum / size;
    }

    function format(n) {
        return String("      " + n).slice(-6);
    }

    Connections {
        target: ApplicationManager
        function onWindowManagerCompositorReadyChanged() {
            for (var i = 0; i < ApplicationManager.count; i++)
                ApplicationManager.application(i).start();
        }
    }
}
