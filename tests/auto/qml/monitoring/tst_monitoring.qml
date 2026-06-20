// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "Monitoring"

    property int spyTimeout: 1500 * AmTest.timeoutFactor

    MonitorModel {
        id: monitor
        running: false
        interval: 100
        maximumCount: 2

        CpuStatus { id: cpu }
        IoStatus { id: io; deviceNames: "null" }
        GpuStatus { id: gpu }
        MemoryStatus { id: mem }
        FrameTimer { id: frames }
    }

    function test_cpu() {
        cpu.update()
        verify(cpu.cpuCores >= 1)
        verify((0 <= cpu.cpuLoad) && (cpu.cpuLoad <= 1))
    }

    function test_io() {
        if (Qt.platform.os !== "linux")
            skip("IO load monitoring is only supported on Linux")

        io.update()
        compare(io.deviceNames.toString(), "null")
        let iol = io.ioLoad
        compare(Object.keys(iol).toString(), "null")
        verify((0 <= iol.null) && (iol.null <= 1))
    }

    function test_gpu() {
        gpu.update()
        verify((0 <= gpu.gpuLoad) && (gpu.gpuLoad <= 1))
    }

    function test_mem() {
        mem.update()
        verify(mem.totalMemory >= 500000000) // 0.5GB
        verify(mem.totalMemory < 500000000000) // 500GB
        verify(mem.memoryUsed > 0)
    }

    FrameContentTracker {
        id: tracker
    }

    FrameTimer {
        id: frameTimer
    }

    SignalSpy {
        id: frameTimerRunningSpy
        target: frameTimer
        signalName: "runningChanged"
    }
    SignalSpy {
        id: frameTimerIntervalSpy
        target: frameTimer
        signalName: "intervalChanged"
    }
    SignalSpy {
        id: frameTimerWindowSpy
        target: frameTimer
        signalName: "windowChanged"
    }
    SignalSpy {
        id: frameTimerUpdatedSpy
        target: frameTimer
        signalName: "updated"
    }

    function test_frameTimer_defaults() {
        compare(frameTimer.roleNames, [ "averageFps", "minimumFps", "maximumFps", "jitterFps" ])
        compare(frameTimer.running, false)
        compare(frameTimer.interval, 1000)
        compare(frameTimer.window, null)
        compare(frameTimer.averageFps, 0)
        compare(frameTimer.minimumFps, 0)
        compare(frameTimer.maximumFps, 0)
        compare(frameTimer.jitterFps, 0)
    }

    function test_frameTimer_interval() {
        frameTimerIntervalSpy.clear()
        frameTimer.interval = 500
        compare(frameTimerIntervalSpy.count, 1)
        compare(frameTimer.interval, 500)

        // setting the same value must not emit again
        frameTimer.interval = 500
        compare(frameTimerIntervalSpy.count, 1)

        frameTimer.interval = 1000
        compare(frameTimerIntervalSpy.count, 2)
    }

    function test_frameTimer_running() {
        frameTimerRunningSpy.clear()
        compare(frameTimer.running, false)

        frameTimer.running = true
        compare(frameTimerRunningSpy.count, 1)
        compare(frameTimer.running, true)

        // idempotent: no change, no signal
        frameTimer.running = true
        compare(frameTimerRunningSpy.count, 1)

        frameTimer.running = false
        compare(frameTimerRunningSpy.count, 2)
        compare(frameTimer.running, false)
    }

    function test_frameTimer_window() {
        frameTimerWindowSpy.clear()
        verify(Window.window)

        // a real toplevel QQuickWindow is accepted
        frameTimer.window = Window.window
        compare(frameTimerWindowSpy.count, 1)
        compare(frameTimer.window, Window.window)

        // setting the same window again is a no-op
        frameTimer.window = Window.window
        compare(frameTimerWindowSpy.count, 1)

        // render a few frames so reportFrameSwap() accumulates timing, then publish via update()
        frameTimerUpdatedSpy.clear()
        for (let i = 0; i < 3; ++i) {
            Window.window.requestUpdate()
            wait(50 * AmTest.timeoutFactor)
        }
        frameTimer.update()
        compare(frameTimerUpdatedSpy.count, 1)
        verify(frameTimer.averageFps >= 0)
        verify(frameTimer.minimumFps >= 0)
        verify(frameTimer.maximumFps >= 0)
        verify(frameTimer.jitterFps >= 0)

        // a non-window QObject is rejected with a warning
        ignoreWarning(/.*The given window is neither a QQuickWindow, ApplicationManagerWindow nor a WindowObject\./)
        frameTimer.window = frameTimer
        compare(frameTimerWindowSpy.count, 2)

        // clearing the window stops tracking
        frameTimer.window = null
        compare(frameTimerWindowSpy.count, 3)
        compare(frameTimer.window, null)
    }

    SignalSpy {
        id: trackerRunningSpy
        target: tracker
        signalName: "runningChanged"
    }
    SignalSpy {
        id: trackerIntervalSpy
        target: tracker
        signalName: "intervalChanged"
    }
    SignalSpy {
        id: trackerWindowSpy
        target: tracker
        signalName: "windowChanged"
    }
    SignalSpy {
        id: trackerUpdatedSpy
        target: tracker
        signalName: "updated"
    }

    function test_frameContentTracker_defaults() {
        compare(tracker.roleNames, [ "duplicateFrames" ])
        compare(tracker.running, false)
        compare(tracker.interval, 1000)
        compare(tracker.window, null)
    }

    function test_frameContentTracker_interval() {
        trackerIntervalSpy.clear()
        tracker.interval = 500
        compare(trackerIntervalSpy.count, 1)
        compare(tracker.interval, 500)

        // setting the same value must not emit again
        tracker.interval = 500
        compare(trackerIntervalSpy.count, 1)

        tracker.interval = 1000
        compare(trackerIntervalSpy.count, 2)
    }

    function test_frameContentTracker_running() {
        trackerRunningSpy.clear()
        compare(tracker.running, false)

        tracker.running = true
        compare(trackerRunningSpy.count, 1)
        compare(tracker.running, true)

        // idempotent: no change, no signal
        tracker.running = true
        compare(trackerRunningSpy.count, 1)

        tracker.running = false
        compare(trackerRunningSpy.count, 2)
        compare(tracker.running, false)
    }

    function test_frameContentTracker_update() {
        trackerUpdatedSpy.clear()
        // each update() emits updated() and resets the counter to 0 for the next period; with no
        // window/rendering attached, the duplicate-frame count settles at 0 after a second update()
        tracker.update()
        compare(trackerUpdatedSpy.count, 1)
        tracker.update()
        compare(trackerUpdatedSpy.count, 2)
        compare(tracker.duplicateFrames, 0)
    }

    function test_frameContentTracker_window() {
        trackerWindowSpy.clear()
        verify(Window.window)

        // a real toplevel QQuickWindow is accepted
        tracker.window = Window.window
        compare(trackerWindowSpy.count, 1)
        compare(tracker.window, Window.window)

        // give the render loop a chance to render frames into the tracker's afterRendering hook
        Window.window.requestUpdate()
        wait(200 * AmTest.timeoutFactor)
        tracker.update()
        verify(tracker.duplicateFrames >= 0)

        // setting the same window again is a no-op
        tracker.window = Window.window
        compare(trackerWindowSpy.count, 1)

        // a non-window QObject is rejected with a warning, but still stored and signalled
        ignoreWarning(/.*The given window is not a QQuickWindow\./)
        tracker.window = tracker
        compare(trackerWindowSpy.count, 2)

        // clearing the window stops tracking
        tracker.window = null
        compare(trackerWindowSpy.count, 3)
        compare(tracker.window, null)
    }

    ProcessStatus {
        id: procStatus
    }

    SignalSpy {
        id: procAppIdSpy
        target: procStatus
        signalName: "applicationIdChanged"
    }
    SignalSpy {
        id: procPidSpy
        target: procStatus
        signalName: "processIdChanged"
    }
    SignalSpy {
        id: procMemEnabledSpy
        target: procStatus
        signalName: "memoryReportingEnabledChanged"
    }
    SignalSpy {
        id: procMemReportingSpy
        target: procStatus
        signalName: "memoryReportingChanged"
    }

    function test_processStatus_defaults() {
        compare(procStatus.roleNames, [ "cpuLoad", "memoryVirtual", "memoryRss", "memoryPss" ])
        compare(procStatus.processId, 0)
        compare(procStatus.cpuLoad, 0)
        compare(procStatus.memoryReportingEnabled, true)
        compare(procStatus.memoryVirtual, {})
        compare(procStatus.memoryRss, {})
        compare(procStatus.memoryPss, {})
    }

    function test_processStatus_memoryReportingEnabled() {
        procMemEnabledSpy.clear()
        procStatus.memoryReportingEnabled = false
        compare(procMemEnabledSpy.count, 1)
        compare(procStatus.memoryReportingEnabled, false)

        // idempotent: no change, no signal
        procStatus.memoryReportingEnabled = false
        compare(procMemEnabledSpy.count, 1)

        procStatus.memoryReportingEnabled = true
        compare(procMemEnabledSpy.count, 2)
        compare(procStatus.memoryReportingEnabled, true)
    }

    function test_processStatus_systemUI() {
        // an empty applicationId means the System UI itself -> our own process
        procAppIdSpy.clear()
        procPidSpy.clear()
        procStatus.applicationId = ""
        compare(procStatus.applicationId, "")
        compare(procAppIdSpy.count, 1)
        // the System UI process is this test process, so the PID is non-zero
        verify(procStatus.processId > 0)
        verify(procPidSpy.count > 0)
    }

    function test_processStatus_invalidAppId() {
        // an unknown application ID warns and resets the PID to 0
        ignoreWarning(/.*Invalid application ID:.*no-such-app.*/)
        procStatus.applicationId = "no-such-app"
        compare(procStatus.applicationId, "no-such-app")
        compare(procStatus.processId, 0)
    }

    function test_processStatus_update() {
        // monitor the System UI's own process
        procStatus.applicationId = ""
        procStatus.memoryReportingEnabled = true
        tryVerify(function() { return procStatus.processId > 0 }, spyTimeout)

        procMemReportingSpy.clear()
        procStatus.update()

        // the worker thread reads /proc and reports back asynchronously
        tryCompare(procMemReportingSpy, "count", 1, spyTimeout, "no memory reporting received")
        verify(procStatus.cpuLoad >= 0)
        // memory reporting is platform dependent: Linux reads all values from
        // /proc, macOS only provides RSS and virtual size, others provide none
        if (Qt.platform.os === "linux") {
            verify(procStatus.memoryPss.total > 0)
            verify(procStatus.memoryRss.total > 0)
            verify(procStatus.memoryVirtual.total > 0)
        } else if (Qt.platform.os === "osx") {
            verify(procStatus.memoryRss.total > 0)
            verify(procStatus.memoryVirtual.total > 0)
        }
    }

    function test_model() {
        compare(monitor.running, false)
        compare(monitor.dataSources.length, 5)
        compare(monitor.dataSources[0], cpu)
        compare(monitor.count, 0)
        compare(monitor.interval, 100)
        verify(Window.window)
        frames.window = Window.window
        monitor.running = true
        compare(monitor.running, true)
        tryVerify(function() { return monitor.count > 0 }, spyTimeout, "no update received")
        verify(monitor.get(0).cpuLoad <= 1)
        tryVerify(function() { return monitor.count == monitor.maximumCount }, spyTimeout, "no update received")
        wait(monitor.interval * 5 * AmTest.timeoutFactor)
        compare(monitor.count, monitor.maximumCount)
        monitor.clear()
        compare(monitor.count, 0)
    }
}
