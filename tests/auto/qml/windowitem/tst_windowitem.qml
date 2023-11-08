// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test

Item {
    id: root
    width: 500
    height: 500
    visible: true

    property int spyTimeout: 5000 * AmTest.timeoutFactor

    Repeater {
        id: windowItemsRepeater
        model: ListModel { id: windowItemsModel }
        delegate: WindowItem {
            id: windowItem
            window: model.window

            property int clickCount: 0
            property alias mouseAreaVisible: mouseArea.visible

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                onClicked: windowItem.clickCount += 1;
                z: -100 // some arbitrary, negative, Z
            }
        }
    }
    Repeater {
        id: sizedWindowItemsRepeater
        model: ListModel { id: sizedWindowItemsModel }
        delegate: WindowItem {
            width: 200
            height: 100
            window: model.window
        }
    }
    Repeater {
        id: noResizeWindowItemsRepeater
        model: ListModel { id: noResizeWindowItemsModel }
        delegate: WindowItem {
            width: 200
            height: 100
            objectFollowsItemSize: false
            window: model.window
        }
    }
    property var chosenModel
    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            root.chosenModel.append({"window":window});
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

    SignalSpy {
        id: objectDestroyedSpy
        target: AmTest
        signalName: "objectDestroyed"
    }

    TestCase {
        id: testCase
        when: windowShown
        name: "WindowItem"

        property var app: null

        function init() {
            compare(windowItemsModel.count, 0);
            compare(sizedWindowItemsModel.count, 0);
            compare(noResizeWindowItemsRepeater.count, 0);
            compare(WindowManager.count, 0);
       }

        function cleanup() {
            windowItemsModel.clear();
            sizedWindowItemsModel.clear();
            noResizeWindowItemsModel.clear();

            if (app)
                app.stop();

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

        function initWindowItemsModel() {
            root.chosenModel = windowItemsModel;
            startAppAndCheckWindow();
        }

        function initSizedWindowItemsModel() {
            root.chosenModel = sizedWindowItemsModel;
            startAppAndCheckWindow();
        }

        function startAppAndCheckWindow() {
            app = ApplicationManager.application("test.windowitem.app");
            app.start();

            tryCompare(root.chosenModel, "count", 1, spyTimeout);
            tryCompare(WindowManager, "count", 1, spyTimeout);
        }

        /*
            The first WindowItem showing a window have primary==true, from the
            second onwards they should have primary==false
         */
        function test_onlyFirstItemIsPrimary() {
            initWindowItemsModel();
            var firstWindowItem = windowItemsRepeater.itemAt(0);
            compare(firstWindowItem.primary, true);

            // Add a second view for the same WindowObject
            windowItemsModel.append({"window":firstWindowItem.window});

            var secondWindowItem = windowItemsRepeater.itemAt(1);
            compare(secondWindowItem.primary, false);
        }

        /*
            Turn a secondary WindowItem (primary == false) into the primary one.

            Check that the previously primary is now secondary and vice-versa.
         */
        function test_turnSecondaryIntoPrimary() {
            initWindowItemsModel();
            var firstWindowItem = windowItemsRepeater.itemAt(0);

            // Add a second view for the same WindowObject
            windowItemsModel.append({"window":firstWindowItem.window});

            var secondWindowItem = windowItemsRepeater.itemAt(1);

            compare(firstWindowItem.primary, true);
            compare(secondWindowItem.primary, false);

            // give primary role to the second WindowItem
            secondWindowItem.makePrimary();

            compare(firstWindowItem.primary, false);
            compare(secondWindowItem.primary, true);

            // and take it back
            firstWindowItem.makePrimary();

            compare(firstWindowItem.primary, true);
            compare(secondWindowItem.primary, false);
        }

        /*
            You have two WindowItems for the same WindowObject

            Check that once the primary WindowItem is destroyed,
            the remaining one takes over the primary role.
         */
        // the function is called 'x*' on purpose: QML tests are executed in alphabetical
        // order and running this directly after the close* functions crashes in RHI
        function test_xdestroyPrimaryRemainingTakesOver() {
            initWindowItemsModel();
            var firstWindowItem = windowItemsRepeater.itemAt(0);

            // Add a second view for the same WindowObject
            windowItemsModel.append({"window":firstWindowItem.window});

            var secondWindowItem = windowItemsRepeater.itemAt(1);

            compare(firstWindowItem.primary, true);
            compare(secondWindowItem.primary, false);

            compare(windowItemsModel.count, 2);

            // destroy the first WindowItem
            windowItemsModel.remove(0 /*index*/, 1 /*count*/);
            firstWindowItem = null;

            compare(windowItemsModel.count, 1);

            // And the remaining item takes over the primary role.
            tryCompare(secondWindowItem, "primary", true, spyTimeout);
        }

        /*
            Check that a WindowObject with state NoSurface is destroyed
            only once all WindowItems using it are gone.
         */
        function test_surfacelessObjectStaysUntilAllItemsAreGone() {
            initWindowItemsModel();
            var firstWindowItem = windowItemsRepeater.itemAt(0);

            // Add a second view for the same WindowObject
            windowItemsModel.append({"window":firstWindowItem.window});

            var secondWindowItem = windowItemsRepeater.itemAt(1);

            var window = WindowManager.get(0).window;
            objectDestroyedSpy.clear();
            var destroyId = AmTest.observeObjectDestroyed(window);

            compare(window.contentState, WindowObject.SurfaceWithContent);
            app.stop();

            // The WindowObject should still exist, albeit without a surface, even though
            // no longer present in WindowManager's model.
            tryCompare(WindowManager, "count", 0, spyTimeout);
            compare(objectDestroyedSpy.count, 0)
            tryCompare(window, "contentState", WindowObject.NoSurface, spyTimeout);

            // Destroy all WindowItems
            firstWindowItem = null;
            secondWindowItem = null;
            windowItemsModel.clear();

            // Now that there are no WindowItems using that WindowObject anymore, it should
            // eventually be deleted by WindowManager
            objectDestroyedSpy.wait(spyTimeout);
            compare(objectDestroyedSpy.signalArguments[0][0], destroyId);
        }

        /*
            Checks that the implicit size of a WindowItem matches the explicit size of the client's ApplicationManagerWindow
         */
        function test_implicitSize() {
            initWindowItemsModel();
            var windowItem = windowItemsRepeater.itemAt(0);
            var window = windowItem.window

            // Must match the ApplicationManagerWindow size defined in apps/test.windowitem.app/mail.qml
            compare(window.size.width, 123);
            compare(window.size.height, 321);
            compare(windowItem.width, 123);
            compare(windowItem.height, 321);

            // Avoid a race condition where the item's implicit size (and therefore the item's
            // size itself as no explicit width or height was assigned) would be set to match
            // the window's size while at the same time an item's resize would make the item resize the window.
            // In short: item size depending on window size while at the same time window size is
            // depending on item size.
            windowItem.objectFollowsItemSize = false;

            var width = 130;
            var height = 330;
            var i;
            for (i = 0; i < 20; i += 1) {
                window.setWindowProperty("requestedWidth", width);
                window.setWindowProperty("requestedHeight", height);

                tryCompare(window, "size", Qt.size(width,height), spyTimeout);
                tryCompare(windowItem, "width", width, spyTimeout);
                tryCompare(windowItem, "height", height, spyTimeout);

                width += 5;
                height += 5;
                wait(50 * AmTest.timeoutFactor);
            }
        }

        /*
            Checks that once a Window is assinged to a WindowItem its underlying surface
            gets resized to match that WindowItem's size (considering it's the first,
            primary, one)
         */
        function test_initialResize() {
            initSizedWindowItemsModel();
            var windowItem = sizedWindowItemsRepeater.itemAt(0);
            var window = windowItem.window

            tryCompare(window, "size", Qt.size(windowItem.width, windowItem.height), spyTimeout);
        }

        /*
            By default a WindowItem will resize the WindowObject it's displaying to match his own size.
            So resizing a WindowItem will cause his WindowObject to follow suit, so that both always
            have matching sizes.
         */
        function test_objectFollowsItemSize() {
            initSizedWindowItemsModel();
            var windowItem = sizedWindowItemsRepeater.itemAt(0);
            var window = windowItem.window;

            windowItem.width = 200;
            windowItem.height = 100;
            tryCompare(window, "size", Qt.size(200, 100), spyTimeout);

            windowItem.width = 201;
            windowItem.height = 101;
            tryCompare(window, "size", Qt.size(201, 101), spyTimeout);

            windowItem.width = 202;
            windowItem.height = 102;
            tryCompare(window, "size", Qt.size(202, 102), spyTimeout);
        }

        /*
            When WindowItem.objectFollowsItemSize is false, resizing the WindowItem will have no effect
            over the WindowObject's size.
         */
        function test_windowDoesNotFolowItemSize() {
            root.chosenModel = noResizeWindowItemsModel;
            startAppAndCheckWindow();

            var windowItem = noResizeWindowItemsRepeater.itemAt(0);
            var window = windowItem.window;

            windowItem.width = 200;
            windowItem.height = 100;
            tryCompare(window, "size", Qt.size(123, 321), spyTimeout);

            windowItem.width = 201;
            windowItem.height = 101;
            tryCompare(window, "size", Qt.size(123, 321), spyTimeout);

            windowItem.width = 202;
            windowItem.height = 102;
            tryCompare(window, "size", Qt.size(123, 321), spyTimeout);
        }

        /*
            Checks that calling close() on an empty WindowObject won't cause a crash (particularly
            in multi-process)
         */
        function test_closeEmptyWindow() {
            initWindowItemsModel();

            var windowItem = windowItemsRepeater.itemAt(0);
            var window = windowItem.window;

            app.stop();

            tryCompare(window, "contentState", WindowObject.NoSurface, spyTimeout);

            window.close();
        }

        /*
            Regression test for https://bugreports.qt.io/browse/AUTOSUITE-652

            - Start an application that has two windows.
            - Call close() on the first one. It should vanish. Application should keep running normally.
            - Call close() on the second one. It should vanish as well and, being the app's last window, it should
              also cause the application to quit.
         */
        function test_closeWindows() {
            root.chosenModel = windowItemsModel;

            app = ApplicationManager.application("test.windowitem.multiwin");
            app.start();

            tryCompare(windowItemsModel, "count", 2, spyTimeout);
            tryCompare(WindowManager, "count", 2, spyTimeout);

            var firstWindow = windowItemsModel.get(0).window;
            var secondWindow = windowItemsModel.get(1).window;

            compare(app.runState, Am.Running);
            compare(firstWindow.contentState, WindowObject.SurfaceWithContent);

            firstWindow.close();

            tryCompare(firstWindow, "contentState", WindowObject.NoSurface, spyTimeout);
            windowItemsModel.remove(0);
            firstWindow = null;

            wait(100 * AmTest.timeoutFactor);

            compare(app.runState, Am.Running);
            compare(secondWindow.contentState, WindowObject.SurfaceWithContent);

            secondWindow.close();

            tryCompare(secondWindow, "contentState", WindowObject.NoSurface, spyTimeout);
            tryCompare(app, "runState", Am.NotRunning, spyTimeout);
        }

        /*
            Children added by System UI code must always stay in front of WindowItem's own private children.
         */
        function test_childrenZOrder() {
            initWindowItemsModel();

            var windowItem = windowItemsRepeater.itemAt(0);
            var window = windowItem.window;

            windowItem.mouseAreaVisible = false;

            touchEvent(windowItem).press(0).commit();
            touchEvent(windowItem).release(0).commit();

            // There's nothing in front of the wayland item (at least nothing visible).
            // The touch event will reach it.
            tryVerify(function() { return window.windowProperty("clickCount") === 1; }, spyTimeout);
            compare(windowItem.clickCount, 0);

            windowItem.mouseAreaVisible = true;

            wait(500) // prevent the next click to be detected as a double-click

            touchEvent(windowItem).press(0).commit();
            touchEvent(windowItem).release(0).commit();

            // Since a visible MouseArea is now in front of WindowItem's internal wayland item
            // the second touch event was caught by that MouseArea instead.
            tryCompare(windowItem, "clickCount", 1, spyTimeout);
            compare(window.windowProperty("clickCount"), 1);

        }

        // Checks that window properties are kept even when contentState is WindowObject.NoSurface
        // Regression test for https://bugreports.qt.io/browse/AUTOSUITE-694
        function test_window_keep_properties_when_nosurface() {
            initWindowItemsModel();

            var window = windowItemsModel.get(0).window;

            compare(window.contentState, WindowObject.SurfaceWithContent);

            window.setWindowProperty("foo", "bar");
            compare(window.windowProperty("foo"), "bar");

            app.stop();

            tryCompare(window, "contentState", WindowObject.NoSurface, spyTimeout);
            compare(window.windowProperty("foo"), "bar");
        }
    }
}
