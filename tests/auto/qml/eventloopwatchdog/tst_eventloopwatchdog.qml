// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "EventLoopWatchdog"
    visible: true
    width: 200
    height: 100

    property int spyTimeout: 10000 * AmTest.timeoutFactor

    function initTestCase() {
        // The event-loop watchdog kills the thread whose event loop is stuck. In single-process
        // mode the apps run in the System-UI's own main thread, so it would kill the test itself.
        if (ApplicationManager.singleProcess)
            skip("The event-loop watchdog can only be tested in multi-process mode");
    }

    function cleanup() {
        // Make sure no (possibly blocked) app leaks into the next test function.
        for (let i = 0; i < ApplicationManager.count; ++i) {
            let a = ApplicationManager.application(i);
            if (a.runState !== Am.NotRunning) {
                a.stop(true /*force*/);
                tryCompare(a, "runState", Am.NotRunning, spyTimeout);
            }
        }
    }

    // An app with a responsive event loop must keep running well past the kill timeout.
    function test_1_responsive_app_survives() {
        let app = ApplicationManager.application("test.watchdog.idle");

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        wait(3000 * AmTest.timeoutFactor); // > killTimeout (2500ms)
        compare(app.runState, Am.Running, "the responsive app was killed unexpectedly");
    }

    // An app that blocks its main thread must have that thread killed by the watchdog. The app
    // is hung and cannot exit on its own, so nothing but the watchdog could have stopped it.
    function test_2_blocked_event_loop_gets_killed() {
        let app = ApplicationManager.application("test.watchdog.hang");

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        tryVerify(() => app.runState !== Am.Running, spyTimeout,
                  "the app blocking its event loop was not killed by the watchdog");
    }
}
