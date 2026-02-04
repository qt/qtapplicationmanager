// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "BubbleWrap"

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property bool appStarted: false
    property var appEnv
    property var netscriptArgs: ([])
    property var mountResult: null

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
            appStarted = true
            appEnv = request.parameters.env;
            request.sendReply({ })
        }
    }

    IntentServerHandler {
        intentIds: [ "netscript-args" ]
        onRequestReceived: (request) => {
            testCase.netscriptArgs.push(request.parameters["args"])
        }
    }

    IntentServerHandler {
        intentIds: "mount-test-result"
        visibility: IntentObject.Public

        onRequestReceived: function(request) {
            testCase.mountResult = request.parameters
            request.sendReply({ })
        }
    }

    function initTestCase() {
        AmTest.runProgram([ "mkdir", "-p", "/tmp/qt-am-bwrap-rw-test" ])
    }

    function cleanupTestCase() {
        AmTest.runProgram([ "rm", "-rf", "/tmp/qt-am-bwrap-rw-test" ])
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

    function startApp(app) {
        appStarted = false
        mountResult = null
        windowAddedSpy.clear()
        runStateChangedSpy.target = app
        app.start()
        windowAddedSpy.wait(spyTimeout)
        tryCompare(testCase, "appStarted", true, spyTimeout)
        tryVerify(function() { return testCase.mountResult !== null }, spyTimeout)
        runStateChangedSpy.clear()
    }

    function stopApp(app) {
        app.stop(false)
        runStateChangedSpy.wait(spyTimeout)    // wait for ShuttingDown
        runStateChangedSpy.wait(spyTimeout)    // wait for NotRunning
        verify(app.runState === Am.NotRunning)
        compare(app.lastExitCode, 0)
    }

    function test_bubblewrap() {
        skipIfUnsupported()

        var app = ApplicationManager.application("TestApp")
        netscriptArgs = []
        startApp(app)

        compare(appEnv["FOO"], "bar");
        compare(appEnv["BAR"], "quoted string");
        compare(appEnv["BAZ"], "1");
        compare(appEnv["BAD"], "");
        compare(appEnv["BAD_TWO"], "");

        stopApp(app)

        tryCompare(testCase.netscriptArgs, "length", 2)
        let netStart = netscriptArgs[0].split(' ')
        let netStop  = netscriptArgs[1].split(' ')
        compare(netStart[0], "start")
        compare(netStop [0], "stop")
        compare(netStart[1], app.id)
        compare(netStop [1], app.id)
        compare(netStart[2], netStop[2])
        verify(netStart[2] !== '')
    }

    function test_customBindMounts() {
        skipIfUnsupported()

        var app = ApplicationManager.application("TestApp")
        startApp(app)

        // ro-bind: marker file inside the ro-mounted directory is readable
        compare(mountResult["ro-marker"].trim(), "ro-test")
        // ${APPLICATION_ID} substitution: the app-id was substituted in the host path
        compare(mountResult["appid-marker"].trim(), "appid-test")
        // rw-bind: writing to the mount point succeeds
        verify(mountResult["rw-writable"])
        // capability-gated mount: absent because TestApp has no 'special-cap' capability
        verify(!mountResult["cap-present"])
        // customBindMounts.app: the app itself is mounted at the configured path
        verify(mountResult["app-path"])

        stopApp(app)
    }

    function test_customBindMountsWithCap() {
        skipIfUnsupported()

        var app = ApplicationManager.application("TestAppWithCap")
        startApp(app)

        // capability-gated mount: present because TestAppWithCap has 'special-cap'
        verify(mountResult["cap-present"])
        compare(mountResult["cap-marker"].trim(), "cap-test")
        // customBindMounts.app: the app itself is mounted at the configured path
        verify(mountResult["app-path"])

        stopApp(app)
    }
}
