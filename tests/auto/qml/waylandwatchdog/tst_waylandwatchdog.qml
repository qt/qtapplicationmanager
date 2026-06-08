// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "WaylandWatchdog"
    visible: true
    width: 200
    height: 100

    property int spyTimeout: 10000 * AmTest.timeoutFactor

    WindowItem {
        id: chrome
        anchors.fill: parent
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            chrome.window = window;
        }
    }

    Connections {
        target: chrome.window
        function onContentStateChanged() {
            if (chrome.window.contentState === WindowObject.NoSurface)
                chrome.window = null;
        }
    }

    function initTestCase() {
        // The Wayland watchdog requires a real compositor and client, which only exist in
        // multi-process mode.
        if (ApplicationManager.singleProcess)
            skip("The Wayland watchdog is only active in multi-process mode");
    }

    function cleanup() {
        // Make sure no (possibly killed or hung) app leaks into the next test function.
        for (let i = 0; i < ApplicationManager.count; ++i) {
            let a = ApplicationManager.application(i);
            if (a.runState !== Am.NotRunning) {
                a.stop(true /*force*/);
                tryCompare(a, "runState", Am.NotRunning, spyTimeout);
            }
        }
    }

    // Wait roughly `ms` milliseconds while repeatedly emitting the event dispatcher's
    // aboutToBlock signal. The compositor only flushes its pending Wayland requests (the
    // outgoing pings) and reads client replies (the pongs) from processWaylandEvents(), which
    // is connected to aboutToBlock - and QtTest's plain wait() does not trigger it
    // (QTBUG-83422). A real System-UI event loop emits aboutToBlock continuously, so this is
    // what makes the test behave like production. (Same workaround as tst_windowmapping /
    // tst_keyinput.)
    function waitFlushingWayland(ms) {
        const step = 100;
        for (let elapsed = 0; elapsed < ms; elapsed += step) {
            AmTest.aboutToBlock();
            wait(step);
        }
    }

    // A responsive client keeps answering the watchdog's pings and must not be killed - even
    // if it stops rendering after its first frame. (A non-rendering client answers pings just
    // fine as long as the compositor actually delivers them.)
    function test_1_responsive_client_survives() {
        let app = ApplicationManager.application("test.watchdog.idle");

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        // Stay alive well past killTimeout (2500ms) to prove it really keeps ponging.
        waitFlushingWayland(3000 * AmTest.timeoutFactor);
        compare(app.runState, Am.Running, "the responsive client was killed unexpectedly");
    }

    // A client that blocks its event loop cannot answer pings and has to be killed.
    function test_2_hung_client_gets_killed() {
        let app = ApplicationManager.application("test.watchdog.hang");

        // emitted by the watchdog once the warn/kill timeouts elapse without a pong
        AmTest.ignoreMessage(AmTest.CriticalMsg, /still hasn't sent a pong reply/);
        AmTest.ignoreMessage(AmTest.CriticalMsg, /is getting killed, because it failed to send a pong reply/);

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        tryVerify(() => { AmTest.aboutToBlock(); return app.runState !== Am.Running; },
                  spyTimeout, "the hung client was not stopped by the watchdog");
    }
}
