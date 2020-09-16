/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

TestCase {
    id: testCase
    when: windowShown
    name: "WindowMapping"
    visible: true

    property var lastWindowAdded;

    WindowItem {
        id: chrome
        anchors.fill: parent

        Connections {
            target: chrome.window
            function onContentStateChanged() {
                if (chrome.window.contentState === WindowObject.NoSurface)
                    chrome.window = null;
            }
        }

        WindowItem {
            id: subChrome
            anchors.fill: parent
            Connections {
                target: subChrome.window
                function onContentStateChanged() {
                    if (subChrome.window.contentState === WindowObject.NoSurface)
                        subChrome.window = null;
                }
            }
        }
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            if (window.windowProperty("type") === "sub")
                subChrome.window = window;
            else
                chrome.window = window;

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

    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function cleanup() {
        runStateChangedSpy.clear();
        ApplicationManager.stopAllApplications();

        while (true) {
            var numRunningApps = 0;
            for (var i = 0; i < ApplicationManager.count; i++) {
                var app = ApplicationManager.application(i);
                if (app.runState !== Am.NotRunning)
                    numRunningApps += 1;
            }

            if (numRunningApps > 0) {
                wait(2000);
            } else
                break;
        }

        windowAddedSpy.clear();
        windowAboutToBeRemovedSpy.clear();
    }

    function test_windowmanager_added_removed_signals() {
        var app = ApplicationManager.application("test.winmap.amwin");

        compare(windowAddedSpy.count, 0);
        app.start("show-main");
        tryCompare(windowAddedSpy, "count", 1);

        compare(windowAboutToBeRemovedSpy.count, 0);
        app.stop();
        tryCompare(windowAboutToBeRemovedSpy, "count", 1);
    }

    function test_amwin_advanced() {
        var app = ApplicationManager.application("test.winmap.amwin2");
        app.start("show-sub");
        wait(2000);
        compare(WindowManager.count, 0);

        app.start("show-main");
        tryCompare(WindowManager, "count", 2);
    }

    function test_amwin_loader() {
        tryCompare(WindowManager, "count", 0);

        var app = ApplicationManager.application("test.winmap.loader");

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2);

        app.start("hide-sub");
        tryCompare(WindowManager, "count", 1);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2);
    }

    function test_amwin_peculiarities() {
        var app = ApplicationManager.application("test.winmap.amwin2");

        tryCompare(WindowManager, "count", 0);

        app.start("show-main");
        tryCompare(WindowManager, "count", 1);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2);

        // Single- vs. multiprocess difference:
        app.start("show-sub2");
        var expectedWindowCount;
        // A Window's effective visible state solely depends on Window hierarchy.
        expectedWindowCount = 3;
        tryCompare(WindowManager, "count", expectedWindowCount);

        app.start("hide-sub");
        expectedWindowCount -= 1;
        tryCompare(WindowManager, "count", expectedWindowCount);

        // Make child (sub) window visible again, parent (main) window is still visible
        app.start("show-sub");
        expectedWindowCount += 1;
        tryCompare(WindowManager, "count", expectedWindowCount);

        // This is weird Window behavior: a child window becomes only visible, when the parent
        // window is visible, but when you change the parent window back to invisible, the child
        // will NOT become invisible.
        app.start("hide-main");
        expectedWindowCount -= 1;
        tryCompare(WindowManager, "count", expectedWindowCount);

        // Single- vs. multiprocess difference:
        app.start("hide-sub");
        if (ApplicationManager.singleProcess) {
            expectedWindowCount -= 1;
        } else {
            // This is even more weird Window behavior: when the parent window is invisible, it is
            // not possible any more to explicitly set the child window to invisible.
            wait(50);
        }
        tryCompare(WindowManager, "count", expectedWindowCount);
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
        verify(chrome.window === null);
        app.start();
        tryCompare(WindowManager, "count", 1);
        tryVerify(function () { return chrome.window !== null });

        app.stop();
        tryCompare(WindowManager, "count", 0);
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
        tryCompare(WindowManager, "count", 1);

        app.start("show-sub");
        tryCompare(WindowManager, "count", 2);

        app.start("hide-sub");
        tryCompare(WindowManager, "count", 1);

        app.stop();
        tryCompare(WindowManager, "count", 0);
    }

    function test_wayland_ping_pong() {
        var app = ApplicationManager.application("test.winmap.ping");

        if (ApplicationManager.singleProcess)
            skip("Wayland ping-pong is only supported in multi-process mode");

        AmTest.ignoreMessage(AmTest.CriticalMsg, /Stopping application.*because we did not receive a Wayland-Pong/);
        app.start();
        tryCompare(app, "runState", Am.Running);
        runStateChangedSpy.clear();
        wait(2200);
        runStateChangedSpy.wait(2000);
        compare(runStateChangedSpy.signalArguments[0][1], Am.ShuttingDown);
        runStateChangedSpy.wait(2000);
        compare(runStateChangedSpy.signalArguments[1][1], Am.NotRunning);
    }

    function test_window_properties() {
        var app = ApplicationManager.application("test.winmap.amwin");

        windowPropertyChangedSpy.clear();
        app.start();
        tryCompare(WindowManager, "count", 1);

        app.start("show-main");
        windowPropertyChangedSpy.wait(2000);
        compare(windowPropertyChangedSpy.count, 1);

        compare(lastWindowAdded.windowProperty("key1"), "val1");
        compare(lastWindowAdded.windowProperty("objectName"), 42);

        lastWindowAdded.setWindowProperty("key2", "val2");
        windowPropertyChangedSpy.wait(2000);
        compare(windowPropertyChangedSpy.count, 2);

        var allProps = lastWindowAdded.windowProperties()
        compare(Object.keys(allProps).length, 3);
        compare(allProps.key1, "val1");
        compare(allProps.key2, "val2");
        compare(allProps.objectName, 42);
    }

    // Checks that window properties survive show/hide cycles
    // Regression test for https://bugreports.qt.io/browse/AUTOSUITE-447
    function test_window_properties_survive_show_hide() {
        var app = ApplicationManager.application("test.winmap.amwin");

        app.start("show-main");
        tryCompare(WindowManager, "count", 1);

        compare(lastWindowAdded.windowProperty("objectName"), 42);

        app.start("hide-main");
        tryCompare(WindowManager, "count", 0);
        app.start("show-main");
        tryCompare(WindowManager, "count", 1);

        compare(lastWindowAdded.windowProperty("objectName"), 42);
    }
}
