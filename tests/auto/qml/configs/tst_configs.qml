// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtTest 1.0
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0


TestCase {
    id: testCase
    when: windowShown
    name: "Configs"
    visible: true

    property int spyTimeout: 5000 * AmTest.timeoutFactor

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
        runStateChangedSpy.wait(spyTimeout);
        runStateChangedSpy.wait(spyTimeout);
        compare(runStateChangedSpy.signalArguments[1][1], ApplicationObject.NotRunning);
    }

    function test_basic_ipc() {
        compare(NotificationManager.count, 0);
        compare(windowAddedSpy.count, 0);
        verify(ApplicationManager.startApplication("test.configs.app"))
        windowAddedSpy.wait(spyTimeout);
        compare(windowAddedSpy.count, 1);
        var window = windowAddedSpy.signalArguments[0][0];
        compare(window.windowProperty("prop1"), "foo");
        IntentClient.sendIntentRequest("test-window-property", "test.configs.app", { })
        windowPropertyChangedSpy.wait(spyTimeout);
        compare(windowPropertyChangedSpy.count, 1);
        compare(windowPropertyChangedSpy.signalArguments[0][0], window);
        compare(window.windowProperty("prop1"), "bar");
        windowPropertyChangedSpy.clear();

        if (!ApplicationManager.systemProperties.nodbus) {
            IntentClient.sendIntentRequest("test-notification", "test.configs.app", { })
            tryVerify(function() { return NotificationManager.count === 1; }, spyTimeout);
            compare(NotificationManager.get(0).summary, "Test");
        }

        window.setWindowProperty("trigger", "now");
        windowPropertyChangedSpy.aboutToBlockWait(spyTimeout);
        compare(windowPropertyChangedSpy.signalArguments[0][0], window);
        compare(window.windowProperty("trigger"), "now");

        windowPropertyChangedSpy.wait(spyTimeout);
        compare(windowPropertyChangedSpy.signalArguments[1][0], window);
        compare(window.windowProperty("ack"), "done");
    }
}
