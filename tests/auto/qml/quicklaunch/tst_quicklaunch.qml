// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

// Tests are meaningless in single-process mode, but still work

TestCase {
    id: testCase
    when: windowShown
    name: "Quicklaunch"

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property bool acknowledged: false

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
        intentIds: "quicklaunch-ready"
        visibility: IntentObject.Public

        onRequestReceived: function(request) {
            acknowledged = true
        }
    }

    function test_quicklaunch() {
        var app = ApplicationManager.application("tld.test.quicklaunch");
        runStateChangedSpy.target = app;

        wait(1000 * AmTest.timeoutFactor);
        // Check for quick-launching is done every second in appman. After 1s now, this test
        // sometimes caused some race where the app would not be started at all in the past:
        app.start();
        windowAddedSpy.wait(spyTimeout);
        tryCompare(testCase, "acknowledged", true, spyTimeout);
        runStateChangedSpy.clear();
        app.stop(true);
        runStateChangedSpy.wait(spyTimeout);    // wait for ShuttingDown
        runStateChangedSpy.wait(spyTimeout);    // wait for NotRunning

        wait(1000 * AmTest.timeoutFactor);
        // Unfortunately there is no reliable means to determine, whether a quicklaunch process
        // is running, but after at least 2s now, there should be a process that can be attached to.
        acknowledged = false;
        app.start();
        windowAddedSpy.wait(spyTimeout);
        tryCompare(testCase, "acknowledged", true, spyTimeout);
        runStateChangedSpy.clear();
        app.stop(true);
        runStateChangedSpy.wait(spyTimeout);    // wait for ShuttingDown
        runStateChangedSpy.wait(spyTimeout);    // wait for NotRunning
    }
}
