// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

Item {
    id: root
    width: 500
    height: 500
    visible: true

    WindowItem {
        id: windowItem
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            windowItem.window = window;
        }

        function onWindowAboutToBeRemoved(window) {
            if (window === windowItem.window) {
                windowItem.window = null;
            }
        }
    }

    // Force redraws so that pings and other events are quickly processed between
    // wayland client and server.
    Rectangle {
        width: 10
        height: 10
        color: "brown"
        RotationAnimator on rotation {
                from: 0; to: 360; duration: 1000
                loops: Animation.Infinite
                running: true
        }
    }

    TestCase {
        id: testCase
        when: windowShown
        name: "WindowItem2"

        function init() {
        }

        function cleanup() {
            waitUntilAllAppsAreStopped();
        }

        function waitUntilAllAppsAreStopped() {
            while (true) {
                var numRunningApps = 0;
                for (var i = 0; i < ApplicationManager.count; i++) {
                    var app = ApplicationManager.application(i);
                    if (app.runState !== Am.NotRunning)
                        numRunningApps += 1;
                }

                if (numRunningApps > 0) {
                    wait(100 * AmTest.timeoutFactor);
                } else
                    break;
            }
        }

        /*
           The sequence below only takes place if WindowManager has direct connections to
           Window::isBeingDisplayedChanged and Window::contentStateChanged and those connections call
           WindowManager::removeWindow:

           1. When Window.contentState changes to Window::NoSurface, WindowManager calls removeWindow() on it.
           2. Inside WindowManager::removeWindow, windowAboutToBeRemoved gets emitted.
           3. The onWindowAboutToBeRemoved signal handler above is called and sets WindowItem.window to null
           4. That causes Window.isBeingDisplayedChanged to turn to false, which in turn gets
              WindowManager to call removeWindow() on that same window once again.
           5. The innermost removeWindow() from 4 successfuly executes and removes the window from WindowManager's
              internal vector
           6. once the outermost removeWindow() from 1. finally goes past the signal emission from 2. and tries
              to remove the item from for index it previously collected, that index is no longer valid as the vector
              already changed. Then you either are removing the wrong item or trying to remove a now invalid index,
              which causes a crash.

            This is a regression test for such crash
         */
        function test_triggerNestedRemoval() {
            var app = ApplicationManager.application("test.windowitem2.app");
            app.start();

            tryVerify(function() { return windowItem.window !== null }, 5000 * AmTest.timeoutFactor);
            wait(100 * AmTest.timeoutFactor);

            app.stop();
        }

    }
}
