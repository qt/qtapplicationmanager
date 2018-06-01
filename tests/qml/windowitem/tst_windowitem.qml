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
    Connections {
        target: WindowManager
        onWindowAdded:  {
            windowItemsModel.append({"window":window});
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
            compare(WindowManager.count, 0);

            app = ApplicationManager.application("test.windowitem.app");
            app.start();

            tryCompare(windowItemsModel, "count", 1);
            tryCompare(WindowManager, "count", 1);
        }

        function cleanup() {
            windowItemsModel.clear();

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

        /*
            The first WindowItem showing a window have primary==true, from the
            second onwards they should have primary==false
         */
        function test_onlyFirstItemIsPrimary() {
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
            var windowItem = windowItemsRepeater.itemAt(0);
            var window = windowItem.window

            // Must match the ApplicationManagerWindow size defined in apps/test.windowitem.app/mail.qml
            compare(window.size.width, 123);
            compare(window.size.height, 321);
            compare(windowItem.width, 123);
            compare(windowItem.height, 321);

            window.setWindowProperty("requestedWidth", 200);
            window.setWindowProperty("requestedHeight", 300);

            tryCompare(window, "size", Qt.size(200,300));
            tryCompare(windowItem, "width", 200);
            tryCompare(windowItem, "height", 300);
        }
    }

}
