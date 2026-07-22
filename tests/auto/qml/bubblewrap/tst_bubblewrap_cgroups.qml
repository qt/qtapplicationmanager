// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test
import CGroupTestHelper

TestCase {
    id: testCase
    when: windowShown
    name: "BubbleWrapCGroups"

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property bool appStarted: false

    // runs in the System-UI process: fakes /sys/fs/cgroup and creates cgroup.procs on demand
    CGroupTestHelper { id: cgroup }

    SignalSpy {
        id: windowAddedSpy
        target: WindowManager
        signalName: "windowAdded"
    }

    SignalSpy {
        id: runStateChangedSpy
        signalName: "runStateChanged"
    }

    IntentServerHandler {
        intentIds: "app-started"
        visibility: IntentObject.Public
        onRequestReceived: function(request) {
            testCase.appStarted = true
            request.sendReply({ })
        }
    }

    // TestApp also sends this on startup; just acknowledge it
    IntentServerHandler {
        intentIds: "mount-test-result"
        visibility: IntentObject.Public
        onRequestReceived: function(request) { request.sendReply({ }) }
    }

    function initTestCase() {
        if (!cgroup.setup("tst-bwrap-cgroup.slice"))
            skip("Test needs a developer-build on Linux")
    }

    function cleanupTestCase() {
        cgroup.cleanup()
    }

    function skipIfUnsupported() {
        if (ApplicationManager.singleProcess)
            skip("Test not supported in single-process mode")
        if (!ApplicationManager.availableContainerIds.includes("bubblewrap"))
            skip("Test not supported without the 'bubblewrap' container being available")

        let bwrapVersionOutput = AmTest.runProgram([ "bwrap", "--version" ]).stdout.trim()
        if (!bwrapVersionOutput.startsWith("bubblewrap "))
            skip("Cannot check the bwrap version")
        let bwrapVersion = bwrapVersionOutput.split(' ')[1].split('.')
        if ((parseInt(bwrapVersion[0]) === 0) && (parseInt(bwrapVersion[1]) < 5))
            skip("Test needs at least bwrap 0.5.0")
    }

    // The container creates a fresh nested cgroup on the fly, the (bwrap) child joins it by writing
    // "0" into cgroup.procs, and the now-empty group is removed again once the app exits.
    function test_cgroupCreatedOnTheFly() {
        skipIfUnsupported()

        var app = ApplicationManager.application("TestApp")
        appStarted = false
        windowAddedSpy.clear()
        runStateChangedSpy.target = app
        app.start()
        windowAddedSpy.wait(spyTimeout)
        tryCompare(testCase, "appStarted", true, spyTimeout)
        runStateChangedSpy.clear()

        // a new nested cgroup was created below the default one and the child joined it
        let nested = cgroup.lastCreatedGroup()
        verify(nested !== "")
        verify(cgroup.groupExists(nested))
        compare(cgroup.readProcs(nested), "0\n")

        // emptying the group (dropping the virtual cgroup.procs) lets the container remove the
        // per-process directory when the process exits
        verify(cgroup.removeProcs(nested))

        app.stop(false)
        runStateChangedSpy.wait(spyTimeout)    // wait for ShuttingDown
        runStateChangedSpy.wait(spyTimeout)    // wait for NotRunning
        verify(app.runState === Am.NotRunning)

        tryVerify(function() { return !cgroup.groupExists(nested) }, spyTimeout)
    }
}
