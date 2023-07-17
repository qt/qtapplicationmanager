// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI

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
        verify(mem.memoryUsed < mem.totalMemory)
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
