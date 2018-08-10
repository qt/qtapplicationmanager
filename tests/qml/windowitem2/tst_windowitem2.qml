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

    WindowItem {
        id: windowItem
    }

    Connections {
        target: WindowManager
        onWindowAdded:  {
            windowItem.window = window;
        }

        onWindowAboutToBeRemoved: {
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
                    wait(50);
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

            tryVerify(function() { return windowItem.window !== null });
            wait(50);

            app.stop();
        }

    }
}
