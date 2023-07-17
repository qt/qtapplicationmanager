// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI

TestCase {
    id: testCase
    when: windowShown
    name: "BubbleWrap"

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property bool appStarted: false
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

        var app = ApplicationManager.application("test.app")
        runStateChangedSpy.target = app

        app.start()
        windowAddedSpy.wait(spyTimeout)
        tryCompare(testCase, "appStarted", true, spyTimeout)
        runStateChangedSpy.clear()

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
