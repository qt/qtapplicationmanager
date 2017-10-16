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
        onWindowReady: window.parent = WindowManager.windowProperty(window, "type") === "sub" ? subChrome : chrome;
        onWindowLost: WindowManager.releaseWindow(window);
    }


    SignalSpy {
        id: windowReadySpy
        target: WindowManager
        signalName: "windowReady"
    }

    SignalSpy {
        id: windowLostSpy
        target: WindowManager
        signalName: "windowLost"
    }

    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function ensureAppTerminated(id) {
        runStateChangedSpy.clear();
        while (ApplicationManager.applicationRunState(id) !== ApplicationManager.NotRunning) {
            runStateChangedSpy.wait(3000);
            verify(runStateChangedSpy.count)
        }
    }

    function test_amwin_advanced() {
        var appId = "test.winmap.amwin2";
        ApplicationManager.startApplication(appId, "show-sub");
        wait(2000);
        compare(windowReadySpy.count, 0);

        ApplicationManager.startApplication(appId, "show-main");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 2);
        windowReadySpy.clear();

        ApplicationManager.stopApplication(appId);
        ensureAppTerminated(appId);
        windowLostSpy.clear();
    }

    function test_amwin_loader() {
        if (!ApplicationManager.singleProcess)
            skip("Sporadically crashes in QtWaylandClient::QWaylandDisplay::flushRequests()");

        var appId = "test.winmap.loader";
        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 2);
        windowReadySpy.clear();

        ApplicationManager.startApplication(appId, "hide-sub");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();

        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        ApplicationManager.stopApplication(appId);
        ensureAppTerminated(appId);
        windowLostSpy.clear();
    }

    function test_amwin_mp_only() {
        if (ApplicationManager.singleProcess)
            skip("Test shows differences in single-process mode (and hence would fail)");

        var appId = "test.winmap.amwin2";
        ApplicationManager.startApplication(appId, "show-main");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        // This part would fail, because sub-window 2 has an invisible Rectangle as parent and
        // hence the effective visible state is false in the current implementation of
        // FakeApplicationManagerWindow. Consequently no windowReady signal will be emitted.
        ApplicationManager.startApplication(appId, "show-sub2");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        // This part would fail, because FakeApplicationManagerWindow will not send
        // windowClosing/windowLost signals when the window is set to invisible.
        ApplicationManager.startApplication(appId, "hide-sub");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();

        // Make child (sub) window visible again, parent (main) window is still visible
        ApplicationManager.startApplication(appId, "show-sub");
        windowReadySpy.wait(3000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();

        // This is weird Window behavior: a child window becomes only visible, when the parent
        // window is visible, but when you change the parent window back to invisible, the child
        // will NOT become invisible.
        ApplicationManager.startApplication(appId, "hide-main");
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();

        // This is even more weird Window behavior: when the parent window is invisible, it is
        // not possible any more to explicitly set the child window to invisible.
        ApplicationManager.startApplication(appId, "hide-sub");
        wait(2000);
        compare(windowLostSpy.count, 0);

        ApplicationManager.stopApplication(appId);
        ensureAppTerminated(appId);
        windowLostSpy.clear();
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

        var appId = data.appId;
        compare(chrome.children.length, 1);
        ApplicationManager.startApplication(appId);
        windowReadySpy.wait(2000);
        compare(windowReadySpy.count, 1);
        windowReadySpy.clear();
        compare(chrome.children.length, 2);

        ApplicationManager.stopApplication(appId);
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, 1);
        windowLostSpy.clear();
        ensureAppTerminated(appId);
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

        var appId = data.appId;
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

        var openWindows = 2;
        // visible handling needs to be fixed for single process mode (AUTOSUITE-131):
        if (!ApplicationManager.singleProcess) {
            ApplicationManager.startApplication(appId, "hide-sub");
            windowLostSpy.wait(2000);
            compare(windowLostSpy.count, 1);
            windowLostSpy.clear();
            openWindows = 1;
            compare(subChrome.children.length, 0);
        }

        ApplicationManager.stopApplication(appId);
        windowLostSpy.wait(2000);
        compare(windowLostSpy.count, openWindows);
        windowLostSpy.clear();
        ensureAppTerminated(appId);
    }
}
