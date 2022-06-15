// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.11
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

TestCase {
    id: testCase
    when: windowShown
    name: "LifeCycleTest"
    visible: true

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property var app: ApplicationManager.application("tld.test.lifecycle");


    WindowItem {
        id: chrome
        anchors.fill: parent
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            chrome.window = window;
        }
    }

    Connections {
        target: chrome.window
        function onContentStateChanged() {
            if (chrome.window.contentState === WindowObject.NoSurface)
                chrome.window = null;
        }
    }


    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    SignalSpy {
        id: objectDestroyedSpy
        target: AmTest
        signalName: "objectDestroyed"
    }

    Timer {
        id: stopTimer
        interval: 1
        onTriggered: app.stop();
    }


    function cleanup() {
        objectDestroyedSpy.clear();
        var index = AmTest.observeObjectDestroyed(app.runtime);
        app.stop();
        while (app.runState !== ApplicationObject.NotRunning)
            runStateChangedSpy.wait(spyTimeout);
        objectDestroyedSpy.wait(spyTimeout);
        compare(objectDestroyedSpy.signalArguments[0][0], index);
    }


    // Start followed by quick stop/start in single-porcess mode caused an abort in the past
    function test_fast_stop_start() {
        app.start();
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.StartingUp);
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.Running);

        objectDestroyedSpy.clear();
        var index = AmTest.observeObjectDestroyed(app.runtime);

        app.stop();
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.ShuttingDown);
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.NotRunning);

        app.start();
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.StartingUp);
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.Running);

        objectDestroyedSpy.wait(spyTimeout);
        compare(objectDestroyedSpy.signalArguments[0][0], index);
    }

    // Quick start/stop followd by start in single-process mode caused an abort in the past
    function test_fast_start_stop() {
        app.start();
        stopTimer.start();

        while (app.runState !== ApplicationObject.NotRunning)
            runStateChangedSpy.wait(spyTimeout);

        app.start();
        while (app.runState !== ApplicationObject.Running)
            runStateChangedSpy.wait(spyTimeout);
    }

    function test_restart() {
        function onRunstateChanged(id, runState) {
            if (runState === Am.NotRunning) {
                ApplicationManager.applicationRunStateChanged.disconnect(onRunstateChanged);
                ApplicationManager.startApplication(id);
            }
        }
        ApplicationManager.applicationRunStateChanged.connect(onRunstateChanged);

        app.start();
        while (app.runState !== ApplicationObject.Running)
            runStateChangedSpy.wait(spyTimeout);
        app.stop();
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.ShuttingDown);
        while (app.runState !== ApplicationObject.Running)
            runStateChangedSpy.wait(spyTimeout);
    }
}
