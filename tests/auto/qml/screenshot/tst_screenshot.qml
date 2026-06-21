// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test
import ScreenshotHelper

// The System UI is a 300x100 blue window. Two 100x100 application windows are placed in it: the
// green app anchored left (x 0..100) and the red app anchored right (x 200..300), leaving the
// blue System UI visible in the 100..200 gap. WindowManager.makeScreenshot() is then exercised
// with the various selector/filename forms and the resulting PNGs are checked for size and the
// expected colors at known coordinates - verifying both that a screenshot was taken and that it
// covers the correct part of the screen.

Window {
    id: root

    readonly property color blue:  "#0000ff"
    readonly property color green: "#00ff00"
    readonly property color red:   "#ff0000"

    readonly property string greenId: "screenshot.green"
    readonly property string redId: "screenshot.red"

    width: 300
    height: 100
    color: blue // blue System UI background

    ScreenshotHelper {
        id: helper
    }

    WindowItem {
        id: greenItem
        x: 0; y: 0; width: 100; height: 100
    }
    WindowItem {
        id: redItem
        x: 200; y: 0; width: 100; height: 100
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            if (window.application.id === root.greenId)
                greenItem.window = window;
            else if (window.application.id === root.redId)
                redItem.window = window;
        }
    }

    // makeScreenshot() of a compositor window uses QQuickWindow::grabWindow(), which captures the
    // window's framebuffer. After an application's surface gets content, the System UI still needs
    // to render a frame for that content to appear in the framebuffer - so we wait for a frame
    // swap before grabbing the compositor window.
    SignalSpy {
        id: frameSwappedSpy
        target: root
        signalName: "frameSwapped"
    }

    TestCase {
        id: testCase
        when: windowShown
        name: "Screenshot"

        property int spyTimeout: 5000 * AmTest.timeoutFactor
        property string dir

        function initTestCase() {
            dir = helper.makeTempDir();
            verify(dir.length > 0);
        }

        function cleanup() {
            ApplicationManager.stopAllApplications();
            while (true) {
                let running = 0;
                for (let i = 0; i < ApplicationManager.count; ++i) {
                    if (ApplicationManager.application(i).runState !== Am.NotRunning)
                        ++running;
                }
                if (running === 0)
                    break;
                wait(100 * AmTest.timeoutFactor);
            }
            // stopping apps only schedules window teardown; wait for Wayland to catch up
            tryCompare(WindowManager, "count", 0, spyTimeout);
            greenItem.window = null;
            redItem.window = null;
        }

        function startApps() {
            let green = ApplicationManager.application(root.greenId);
            let red = ApplicationManager.application(root.redId);
            green.start();
            red.start();
            tryCompare(WindowManager, "count", 2, spyTimeout);
            tryVerify(() => { return greenItem.window !== null && redItem.window !== null }, spyTimeout);
            // wait until both surfaces actually carry content, so the grab isn't empty
            tryVerify(() => { return greenItem.window.contentState === WindowObject.SurfaceWithContent
                                  && redItem.window.contentState === WindowObject.SurfaceWithContent },
                      spyTimeout);

            // ensure the System UI has rendered at least one frame containing that content into
            // its framebuffer, so a subsequent grabWindow() captures the apps' colors
            frameSwappedSpy.clear();
            root.update();
            frameSwappedSpy.wait(spyTimeout);
            wait(100 * AmTest.timeoutFactor); // sometimes it takes two frames, if Wayland is slow
        }

        // an empty selector captures the (single) compositor window: the whole 300x100 image,
        // with green on the left, blue in the middle and red on the right
        // (the testrunner pins QT_SCALE_FACTOR=1, so the grab is exactly the logical size)
        function test_compositorWindow() {
            startApps();

            let file = dir + "/comp-%s.png";
            let expected = dir + "/comp-0.png";
            verify(WindowManager.makeScreenshot(file, ""));
            tryVerify(() => { return helper.fileExists(expected) }, spyTimeout);

            compare(helper.sizeOf(expected), Qt.size(300, 100));
            compare(helper.colorAt(expected, 50, 50), root.green);   // left -> green app
            compare(helper.colorAt(expected, 150, 50), root.blue);   // gap  -> blue System UI
            compare(helper.colorAt(expected, 250, 50), root.red);    // right -> red app
        }

        // an exact application id captures only that app's window
        function test_singleApp() {
            startApps();

            let file = dir + "/app-%i-%w.png";
            verify(WindowManager.makeScreenshot(file, root.greenId));

            // %w is the window's index in the WindowManager model, which we can look up directly
            let greenWinIdx = WindowManager.indexOfWindow(greenItem.window);
            let expected = dir + "/app-" + root.greenId + "-" + greenWinIdx + ".png";
            tryVerify(() => { return helper.fileExists(expected) }, spyTimeout);
            compare(helper.sizeOf(expected), Qt.size(100, 100));
            compare(helper.colorAt(expected, 50, 50), root.green);
        }

        // a wildcard id captures every matching application window
        function test_wildcard() {
            startApps();

            let file = dir + "/wild-%i.png";
            verify(WindowManager.makeScreenshot(file, "screenshot.*"));

            let greenFile = dir + "/wild-" + root.greenId + ".png";
            let redFile = dir + "/wild-" + root.redId + ".png";
            tryVerify(() => { return helper.fileExists(greenFile) && helper.fileExists(redFile) }, spyTimeout);
            compare(helper.colorAt(greenFile, 50, 50), root.green);
            compare(helper.colorAt(redFile, 50, 50), root.red);
        }

        // a window-property selector restricts the capture to matching windows
        function test_windowPropertySelector() {
            startApps();

            let file = dir + "/prop-%i.png";
            verify(WindowManager.makeScreenshot(file, "[role=right]"));

            let redFile = dir + "/prop-" + root.redId + ".png";
            tryVerify(() => { return helper.fileExists(redFile) }, spyTimeout);
            compare(helper.colorAt(redFile, 50, 50), root.red);

            // the green window (role=left) must not have been captured
            wait(200 * AmTest.timeoutFactor);
            verify(!helper.fileExists(dir + "/prop-" + root.greenId + ".png"));
        }

        // a selector matching no application returns false and writes nothing
        function test_noMatch() {
            startApps();

            let file = dir + "/none-%i.png";
            verify(!WindowManager.makeScreenshot(file, "does.not.exist"));
        }

        // the %% sequence is replaced by a literal percent sign
        function test_literalPercent() {
            startApps();

            let file = dir + "/lit-%%-%s.png";
            let expected = dir + "/lit-%-0.png";
            verify(WindowManager.makeScreenshot(file, ""));
            tryVerify(() => { return helper.fileExists(expected) }, spyTimeout);
            compare(helper.sizeOf(expected), Qt.size(300, 100));
        }
    }
}
