/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
import QtApplicationManager 1.0

Item {
    id: root
    width: 500
    height: 500
    visible: true

    Repeater {
        id: windowItemsRepeater
        model: ListModel { id: windowItemsModel }
        delegate: WindowItem {
            window: model.window
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
        onWindowAdded:  {
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

            // TODO: If you call stop() on the same application twice,
            // WindowManager::inProcessSurfaceItemClosing will be called twice, and the
            // second time it's called the surfaceItem there will be a dangling pointer.
            // So we're avoding calling app.stop() twice here.
            // See comment in InProcessWindow::~InProcessWindow() for more information
            if (app) {
                app.stop();
                app = null;
            }

            waitUntilAllAppsAreStopped();
        }

        function waitUntilAllAppsAreStopped() {
            while (true) {
                var numRunningApps = 0;
                for (var i = 0; i < ApplicationManager.count; i++) {
                    var app = ApplicationManager.application(i);
                    if (app.runState !== ApplicationObject.NotRunning)
                        numRunningApps += 1;
                }

                if (numRunningApps > 0) {
                    wait(50);
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

            tryCompare(root.chosenModel, "count", 1);
            tryCompare(WindowManager, "count", 1);
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
        function test_destroyPrimaryRemainingTakesOver() {
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
            tryCompare(secondWindowItem, "primary", true);
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

            compare(window.contentState, WindowObject.SurfaceWithContent);
            app.stop();
            app = null;
            wait(50); // give it some time for any queued events/emissions to be processed.

            // The WindowObject should still be there, albeit without a surface
            compare(WindowManager.count, 1);
            tryCompare(window, "contentState", WindowObject.NoSurface);

            // Destroy all WindowItems
            firstWindowItem = null;
            secondWindowItem = null;
            windowItemsModel.clear();

            // Now that there are no WindowItems using that WindowObject anymore, it should
            // eventually be deleted by WindowManager
            tryCompare(WindowManager, "count", 0);
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

                tryCompare(window, "size", Qt.size(width,height));
                tryCompare(windowItem, "width", width);
                tryCompare(windowItem, "height", height);

                width += 5;
                height += 5;
                wait(10);
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

            tryCompare(window, "size", Qt.size(windowItem.width, windowItem.height));
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
            tryCompare(window, "size", Qt.size(200, 100));

            windowItem.width = 201;
            windowItem.height = 101;
            tryCompare(window, "size", Qt.size(201, 101));

            windowItem.width = 202;
            windowItem.height = 102;
            tryCompare(window, "size", Qt.size(202, 102));
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
            tryCompare(window, "size", Qt.size(123, 321));

            windowItem.width = 201;
            windowItem.height = 101;
            tryCompare(window, "size", Qt.size(123, 321));

            windowItem.width = 202;
            windowItem.height = 102;
            tryCompare(window, "size", Qt.size(123, 321));
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
            app = null;

            tryCompare(window, "contentState", WindowObject.NoSurface);

            window.close();
        }
    }
}
