// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "WindowMapping"
    visible: true
    width: 300
    height: 100

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property var lastWindowAdded

    component WindowChrome: WindowItem {
        id: chrome
        width: 100; height: 100

        Connections {
            target: chrome.window
            function onContentStateChanged() {
                if (chrome.window.contentState === WindowObject.NoSurface)
                    chrome.window = null;
            }
        }
    }

    WindowChrome {
        id: topChrome
    }
    WindowChrome {
        id: subChrome
        x: 100
    }
    WindowChrome {
        id: sub2Chrome
        x: 200
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            if (window.windowProperty("type") === "sub")
                subChrome.window = window;
            else if (window.windowProperty("type") === "sub2")
                sub2Chrome.window = window;
            else
                topChrome.window = window;

            testCase.lastWindowAdded = window;
        }
    }


    SignalSpy {
        id: windowAddedSpy
        target: WindowManager
        signalName: "windowAdded"
    }

    SignalSpy {
        id: windowAboutToBeRemovedSpy
        target: WindowManager
        signalName: "windowAboutToBeRemoved"
    }

    SignalSpy {
        id: windowPropertyChangedSpy
        target: WindowManager
        signalName: "windowPropertyChanged"
    }

    FrameTimer {
        id: frameTimer
    }

    SignalSpy {
        id: frameTimerWindowSpy
        target: frameTimer
        signalName: "windowChanged"
    }

    function cleanup() {
        ApplicationManager.stopAllApplications();

        while (true) {
            var numRunningApps = 0;
            for (var i = 0; i < ApplicationManager.count; i++) {
                var app = ApplicationManager.application(i);
                if (app.runState !== Am.NotRunning)
                    numRunningApps += 1;
            }

            if (numRunningApps == 0)
                break;

            wait(100);
        }

        windowAddedSpy.clear();
        windowAboutToBeRemovedSpy.clear();
    }

    function test_windowmanager_added_removed_signals() {
        var app = ApplicationManager.application("test.winmap.amwin");

        compare(windowAddedSpy.count, 0);
        app.start("show-main");
        tryCompare(windowAddedSpy, "count", 1, spyTimeout);

        compare(windowAboutToBeRemovedSpy.count, 0);
        app.stop();
        tryCompare(windowAboutToBeRemovedSpy, "count", 1, spyTimeout);
    }

    function test_amwin_advanced() {
        var app = ApplicationManager.application("test.winmap.amwin2");
        // showing a sub-window while its parent is still hidden must not map any window: wait for
        // the app to start, then flush the compositor (see QTBUG-83422) so that an erroneous
        // window-add would have surfaced by now - without blindly waiting a fixed duration
        app.start("show-sub");
        tryCompare(app, "runState", Am.Running, spyTimeout);
        AmTest.aboutToBlock();
        wait(250 * AmTest.timeoutFactor);
        compare(WindowManager.count, 0);

        app.start("show-main");
        tryCompare(WindowManager, "count", 2, spyTimeout);
    }

    function test_amwin_loader() {
        tryCompare(WindowManager, "count", 0, spyTimeout);

        var app = ApplicationManager.application("test.winmap.loader");

        app.start("create-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        app.start("destroy-sub");
        tryCompare(WindowManager, "count", 1, spyTimeout);

        app.start("create-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);
    }

    function test_amwin_close() {
        var app = ApplicationManager.application("test.winmap.amwin2");

        tryCompare(WindowManager, "count", 0, spyTimeout);

        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);

        topChrome.window.close()
        AmTest.aboutToBlock();   // see QTBUG-83422
        tryCompare(WindowManager, "count", 0, spyTimeout);

        // last window closed -> exit
        tryCompare(app, "runState", Am.NotRunning, spyTimeout);

        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        subChrome.window.close();
        AmTest.aboutToBlock();
        tryCompare(WindowManager, "count", 1, spyTimeout);
    }

    function test_quitOnLastClosed() {
        var app = ApplicationManager.application("test.winmap.amwin2");

        tryCompare(WindowManager, "count", 0, spyTimeout);
        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);
        compare(app.runState, Am.Running);
        topChrome.window.close();
        AmTest.aboutToBlock();
        tryCompare(app, "runState", Am.NotRunning, spyTimeout);

        tryCompare(WindowManager, "count", 0, spyTimeout);
        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);
        app.start("show-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);
        app.start("show-sub2");
        tryCompare(WindowManager, "count", 3, spyTimeout);
        compare(app.runState, Am.Running);
        topChrome.window.close();
        AmTest.aboutToBlock();
        tryCompare(app, "runState", Am.NotRunning, spyTimeout);

        tryCompare(WindowManager, "count", 0, spyTimeout);
        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);
        app.start("show-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);
        tryVerify(function() { return subChrome != null }, spyTimeout);
        compare(app.runState, Am.Running);
        subChrome.window.close();
        AmTest.aboutToBlock();
        tryCompare(subChrome, "window", null, spyTimeout);
        // closing a sub-window while the main window stays open must not quit the app: give a
        // spurious quit a chance to surface, then confirm the app is still running
        wait(250 * AmTest.timeoutFactor);
        compare(app.runState, Am.Running);
    }

    function test_default_data() {
        return [ { tag: "ApplicationManagerWindow", appId: "test.winmap.amwin" },
                 // skipping QtObject, as it doesn't show anything
                 { tag: "Rectangle", appId: "test.winmap.rectangle" },
                 { tag: "Window", appId: "test.winmap.window" } ];
    }

    function test_default(data) {
        if (ApplicationManager.singleProcess && data.tag === "Window")
            skip("Window root element is not properly supported in single process mode.");

        compare(WindowManager.count, 0);

        var app = ApplicationManager.application(data.appId);
        verify(topChrome.window === null);
        app.start();
        tryCompare(WindowManager, "count", 1, spyTimeout);
        tryVerify(function () { return topChrome.window !== null }, spyTimeout);

        app.stop();
        tryCompare(WindowManager, "count", 0, spyTimeout);
    }

    function test_mapping_data() {
        return [ { tag: "ApplicationManagerWindow", appId: "test.winmap.amwin" },
                 { tag: "QtObject", appId: "test.winmap.qtobject" },
                 { tag: "Rectangle", appId: "test.winmap.rectangle" },
                 { tag: "Window", appId: "test.winmap.window" } ];
    }

    function test_mapping(data) {
        if (ApplicationManager.singleProcess && data.tag === "Window")
            skip("Window root element is not properly supported in single process mode.");

        var app = ApplicationManager.application(data.appId);

        compare(WindowManager.count, 0);

        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        app.start("hide-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        app.start("hide-sub");
        tryCompare(WindowManager, "count", 2, spyTimeout);

        app.stop();
        tryCompare(WindowManager, "count", 0, spyTimeout);
    }

    function test_window_properties() {
        var app = ApplicationManager.application("test.winmap.amwin");

        windowPropertyChangedSpy.clear();
        app.start();
        tryCompare(WindowManager, "count", 1, spyTimeout);

        app.start("show-main");
        windowPropertyChangedSpy.wait(spyTimeout);
        compare(windowPropertyChangedSpy.count, 1);

        compare(lastWindowAdded.windowProperty("key1"), "val1");
        compare(lastWindowAdded.windowProperty("objectName"), 42);

        lastWindowAdded.setWindowProperty("key2", "val2");
        windowPropertyChangedSpy.wait(spyTimeout);
        compare(windowPropertyChangedSpy.count, 2);

        var allProps = lastWindowAdded.windowProperties()
        compare(Object.keys(allProps).length, 3);
        compare(allProps.key1, "val1");
        compare(allProps.key2, "val2");
        compare(allProps.objectName, 42);
    }

    // Starting with Qt 6.9, hide() does not delete the Wayland surface anymore
    function test_window_show_hide() {
        let app = ApplicationManager.application("test.winmap.amwin");

        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);
        compare(topChrome.window.contentState, WindowObject.SurfaceWithContent);
        compare(lastWindowAdded.windowProperty("objectName"), 42);
        let img = grabImage(topChrome.Window.contentItem);
        compare(img.pixel(1,1), "#fea500")

        app.start("hide-main");
        tryCompare(topChrome.window, "contentState", WindowObject.SurfaceNoContent, spyTimeout);
        compare(WindowManager.count, 1);
        img = grabImage(topChrome.Window.contentItem);
        compare(img.pixel(1,1), "#ffffff")

        app.start("show-main");
        tryCompare(topChrome.window, "contentState", WindowObject.SurfaceWithContent, spyTimeout);
        compare(WindowManager.count, 1);
        compare(lastWindowAdded.windowProperty("objectName"), 42);
    }

    function test_subsurface_position() {
        if (ApplicationManager.singleProcess)
            skip("Can't grab real window in single-process mode.");
        ApplicationManager.application("test.winmap.subsurface").start();
        tryCompare(WindowManager, "count", 1, spyTimeout);
        compare(topChrome.window.contentState, WindowObject.SurfaceWithContent);
        const img = grabImage(topChrome.Window.contentItem);
        compare(img.pixel(60,40), Qt.color("cyan"));
    }

    // Exercises SystemFrameTimerImpl, which is the FrameTimer backend used when a FrameTimer is
    // pointed at a WindowObject (as opposed to a plain QQuickWindow). The WindowObject is an
    // InProcessWindow in single-process mode and a WaylandWindow in multi-process mode, so this
    // covers both branches of SystemFrameTimerImpl::connectToSystemWindow() across the two test
    // configurations, plus disconnectFromSystemWindow() when the window is cleared.
    function test_frameTimer_systemWindow() {
        var app = ApplicationManager.application("test.winmap.amwin");

        compare(WindowManager.count, 0);
        app.start("show-main");
        tryCompare(WindowManager, "count", 1, spyTimeout);
        tryVerify(function() { return topChrome.window !== null }, spyTimeout);

        frameTimerWindowSpy.clear();

        if (ApplicationManager.singleProcess) {
            // an InProcessWindow cannot have its FPS measured: the impl warns but still accepts it
            ignoreWarning(/.*It makes no sense to measure the FPS of a WindowObject in single-process mode.*/);
        }
        frameTimer.window = topChrome.window;
        compare(frameTimerWindowSpy.count, 1);
        compare(frameTimer.window, topChrome.window);

        // setting the same window again is a no-op
        frameTimer.window = topChrome.window;
        compare(frameTimerWindowSpy.count, 1);

        if (!ApplicationManager.singleProcess) {
            // in multi-process mode the impl hooks the Wayland surface's redraw signal; render a
            // few frames and publish them, then check that we get sane values
            for (let i = 0; i < 3; ++i) {
                app.start("hide-main");
                tryCompare(topChrome.window, "contentState", WindowObject.SurfaceNoContent, spyTimeout);
                app.start("show-main");
                tryCompare(topChrome.window, "contentState", WindowObject.SurfaceWithContent, spyTimeout);
            }
            frameTimer.update();
            verify(frameTimer.averageFps >= 0);
        }

        // clearing the window exercises disconnectFromSystemWindow()
        frameTimer.window = null;
        compare(frameTimerWindowSpy.count, 2);
        compare(frameTimer.window, null);

        app.stop();
        tryCompare(WindowManager, "count", 0, spyTimeout);
    }
}
