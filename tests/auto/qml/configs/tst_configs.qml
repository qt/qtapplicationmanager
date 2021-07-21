/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.4
import QtTest 1.0
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0


TestCase {
    id: testCase
    when: windowShown
    name: "Configs"
    visible: true


    IntentServerHandler {
         intentIds: "system-func"
         visibility: IntentObject.Public

         onRequestReceived: function(request) {
             let str = request.parameters["str"]
             request.sendReply({ "int": str === "bar" ? 42: 0 })
         }
    }

    SignalSpy {
        id: windowAddedSpy
        target: WindowManager
        signalName: "windowAdded"
    }

    SignalSpy {
        id: windowPropertyChangedSpy
        // Workaround to flush Wayland messages, see https://bugreports.qt.io/browse/AUTOSUITE-709
        // A proper solution in QtWayland is sought here: https://bugreports.qt.io/browse/QTBUG-83422
        function aboutToBlockWait(timeout)
        {
            AmTest.aboutToBlock();
            wait(timeout);
        }
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
        ApplicationManager.stopApplication("test.configs.app");
        runStateChangedSpy.wait();
        runStateChangedSpy.wait();
        compare(runStateChangedSpy.signalArguments[1][1], ApplicationObject.NotRunning);
    }

    function test_basic_ipc() {
        compare(NotificationManager.count, 0);
        compare(windowAddedSpy.count, 0);
        verify(ApplicationManager.startApplication("test.configs.app"))
        windowAddedSpy.wait();
        compare(windowAddedSpy.count, 1);
        var window = windowAddedSpy.signalArguments[0][0];
        compare(window.windowProperty("prop1"), "foo");
        IntentClient.sendIntentRequest("test-window-property", "test.configs.app", { })
        windowPropertyChangedSpy.wait();
        compare(windowPropertyChangedSpy.count, 1);
        compare(windowPropertyChangedSpy.signalArguments[0][0], window);
        compare(window.windowProperty("prop1"), "bar");
        windowPropertyChangedSpy.clear();

        if (!ApplicationManager.systemProperties.nodbus) {
            IntentClient.sendIntentRequest("test-notification", "test.configs.app", { })
            tryVerify(function() { return NotificationManager.count === 1; });
            compare(NotificationManager.get(0).summary, "Test");
        }

        window.setWindowProperty("trigger", "now");
        windowPropertyChangedSpy.aboutToBlockWait();
        compare(windowPropertyChangedSpy.signalArguments[0][0], window);
        compare(window.windowProperty("trigger"), "now");

        windowPropertyChangedSpy.wait();
        compare(windowPropertyChangedSpy.signalArguments[1][0], window);
        compare(window.windowProperty("ack"), "done");
    }
}
