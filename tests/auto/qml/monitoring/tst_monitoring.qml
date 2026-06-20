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
