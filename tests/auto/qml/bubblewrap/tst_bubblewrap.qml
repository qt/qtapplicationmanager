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

    function test_bubblewrap() {
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

        var app = ApplicationManager.application("TestApp")
        runStateChangedSpy.target = app

        app.start()
        windowAddedSpy.wait(spyTimeout)
        tryCompare(testCase, "appStarted", true, spyTimeout)
        runStateChangedSpy.clear()

        compare(appEnv["FOO"], "bar");
        compare(appEnv["BAR"], "quoted string");
        compare(appEnv["BAZ"], "1");
        compare(appEnv["BAD"], "");
        compare(appEnv["BAD_TWO"], "");

        app.stop(false)
        runStateChangedSpy.wait(spyTimeout)    // wait for ShuttingDown
        runStateChangedSpy.wait(spyTimeout)    // wait for NotRunning

        verify(app.runState === Am.NotRunning)
        compare(app.lastExitCode, 0)

        tryCompare(testCase.netscriptArgs, "length", 2)
        let netStart = netscriptArgs[0].split(' ')
        let netStop = netscriptArgs[1].split(' ')
        compare(netStart[0], "start")
        compare(netStop [0], "stop")
        compare(netStart[1], app.id)
        compare(netStop [1], app.id)
        compare(netStart[2], netStop[2])
        verify(netStart[2] !== '')
    }
}
