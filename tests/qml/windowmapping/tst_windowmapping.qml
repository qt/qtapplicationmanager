/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

TestCase {
    id: testCase
    when: windowShown
    name: "WindowMapping"
    visible: true

    property string appId;
    property Item lastWindowReady;

    Item {
        id: chrome
        anchors.fill: parent

        Item {
            id: subChrome
            anchors.fill: parent
        }
    }

    Connections {
        target: WindowManager
        onWindowReady: {
            window.parent = WindowManager.windowProperty(window, "type") === "sub" ? subChrome : chrome;
            lastWindowReady = window;
        }
        onWindowLost: WindowManager.releaseWindow(window);
    }


    SignalSpy {
        id: windowReadySpy
        target: WindowManager
        signalName: "windowReady"
    }

    SignalSpy {
        id: windowClosingSpy
        target: WindowManager
        signalName: "windowClosing"
    }

    SignalSpy {
        id: windowLostSpy
        target: WindowManager
        signalName: "windowLost"
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
        ApplicationManager.stopApplication(appId);
        while (ApplicationManager.applicationRunState(appId) !== ApplicationManager.NotRunning)
            runStateChangedSpy.wait(3000);
        windowReadySpy.clear();
        windowClosingSpy.clear();
        windowLostSpy.clear();
    }


    function test_amwin_advanced() {
        appId = "test.winmap.amwin2";
        ApplicationManager.startApplication(appId, "show-sub");
        wait(2000);
        compare(windowReadySpy.count, 0);

        ApplicationManager.startApplication(appId, "show-main");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 2);
    }

    function test_amwin_loader() {
        if (!ApplicationManager.singleProcess)
            skip("Sporadically crashes in QtWaylandClient::QWaylandDisplay::flushRequests()");

        appId = "test.winmap.loader";
        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 2);
        windowReadySpy.clear();

        ApplicationManager.startApplication(appId, "hide-sub");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);

        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
    }

    function test_amwin_peculiarities() {
        appId = "test.winmap.amwin2";
        ApplicationManager.startApplication(appId, "show-main");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        // Single- vs. multiprocess difference:
        ApplicationManager.startApplication(appId, "show-sub2");
        if (ApplicationManager.singleProcess) {
            // Sub-window 2 has an invisible Rectangle as parent and hence the effective
            // visible state is false. Consequently no windowReady signal will be emitted.
            wait(2000);
            compare(windowReadySpy.count, 0);
        } else {
            // A Window's effective visible state solely depends on Window hierarchy.
            windowReadySpy.wait(3000);
            compare(windowReadySpy.count, 1);
            windowReadySpy.clear();
        }

        ApplicationManager.startApplication(appId, "hide-sub");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();

        // Make child (sub) window visible again, parent (main) window is still visible
        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);

        // This is weird Window behavior: a child window becomes only visible, when the parent
        // window is visible, but when you change the parent window back to invisible, the child
        // will NOT become invisible.
        ApplicationManager.startApplication(appId, "hide-main");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();

        // Single- vs. multiprocess difference:
        ApplicationManager.startApplication(appId, "hide-sub");
        if (ApplicationManager.singleProcess) {
            windowLostSpy.wait(2000);
            compare(windowLostSpy.count, 1);
        } else {
            // This is even more weird Window behavior: when the parent window is invisible, it is
            // not possible any more to explicitly set the child window to invisible.
            wait(2000);
            compare(windowLostSpy.count, 0);
        }
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

        appId = data.appId;
        compare(chrome.children.length, 1);
        ApplicationManager.startApplication(appId);
        windowReadySpy.wait(2000);
        compare(windowReadySpy.count, 1);
        compare(chrome.children.length, 2);

        ApplicationManager.stopApplication(appId);
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
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

        appId = data.appId;
        compare(chrome.children.length, 1);
        ApplicationManager.startApplication(appId, "show-main");
        windowReadySpy.wait(2000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();
        compare(chrome.children.length, 2);

        compare(subChrome.children.length, 0);
        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(2000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();
        compare(subChrome.children.length, 1);

        ApplicationManager.startApplication(appId, "hide-sub");
        windowClosingSpy.wait(2000);
        compare(windowClosingSpy.count, 1);
        windowClosingSpy.clear();
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();
        compare(subChrome.children.length, 0);

        ApplicationManager.stopApplication(appId);
        windowClosingSpy.wait(2000);
        compare(windowClosingSpy.count, 1);
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
    }

    function test_wayland_ping_pong() {
        appId = "test.winmap.ping";
        if (ApplicationManager.singleProcess)
            skip("Wayland ping-pong is only supported in multi-process mode");
        ApplicationManager.startApplication(appId);
        windowReadySpy.wait(2000);
        compare(ApplicationManager.applicationRunState(appId), ApplicationManager.Running)
        runStateChangedSpy.clear();
        wait(2200);
        runStateChangedSpy.wait(2000);
        compare(runStateChangedSpy.signalArguments[0][1], ApplicationManager.ShuttingDown)
        runStateChangedSpy.wait(2000);
        compare(runStateChangedSpy.signalArguments[1][1], ApplicationManager.NotRunning)
    }

    function test_window_properties() {
        appId = "test.winmap.amwin";
        ApplicationManager.startApplication(appId);
        windowReadySpy.wait(2000);
        compare(windowReadySpy.count, 1);

        ApplicationManager.startApplication(appId, "show-main");
        windowPropertyChangedSpy.wait(2000);
        compare(windowPropertyChangedSpy.count, 1);

        compare(WindowManager.windowProperty(lastWindowReady, "key1"), "val1");
        compare(WindowManager.windowProperty(lastWindowReady, "objectName"), 42);

        WindowManager.setWindowProperty(lastWindowReady, "key2", "val2");
        windowPropertyChangedSpy.wait(2000);
        compare(windowPropertyChangedSpy.count, 2);

        var allProps = WindowManager.windowProperties(lastWindowReady)
        compare(Object.keys(allProps).length, 3);
        compare(allProps.key1, "val1");
        compare(allProps.key2, "val2");
        compare(allProps.objectName, 42);
    }
}
