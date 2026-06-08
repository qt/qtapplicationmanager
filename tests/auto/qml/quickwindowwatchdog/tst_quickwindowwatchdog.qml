// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "QuickWindowWatchdog"
    visible: true
    width: 200
    height: 100

    property int spyTimeout: 10000 * AmTest.timeoutFactor

    // The apps must be composited so their render loop keeps producing frames.
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
        // The quick-window watchdog kills the render thread of the offending window. In
        // single-process mode that would be the System-UI's own render thread.
        if (ApplicationManager.singleProcess)
            skip("The quick-window watchdog can only be tested in multi-process mode");
    }

    function cleanup() {
        // Make sure no (possibly render-blocked) app leaks into the next test function.
        for (let i = 0; i < ApplicationManager.count; ++i) {
            let a = ApplicationManager.application(i);
            if (a.runState !== Am.NotRunning) {
                a.stop(true /*force*/);
                tryCompare(a, "runState", Am.NotRunning, spyTimeout);
            }
        }
    }

    // A continuously rendering app must keep running well past the kill timeout.
    function test_1_rendering_app_survives() {
        let app = ApplicationManager.application("test.watchdog.animated");

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        wait(3000 * AmTest.timeoutFactor); // > killTimeout (2500ms)
        compare(app.runState, Am.Running, "the rendering app was killed unexpectedly");
    }

    // An app that blocks inside its rendering phase must have its render thread killed by the
    // watchdog. The app is stuck and cannot exit on its own, so nothing but the watchdog could
    // have stopped it.
    function test_2_blocked_render_thread_gets_killed() {
        let app = ApplicationManager.application("test.watchdog.renderblock");

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        tryVerify(() => app.runState !== Am.Running, spyTimeout,
                  "the app blocking its render thread was not killed by the watchdog");
    }
}
