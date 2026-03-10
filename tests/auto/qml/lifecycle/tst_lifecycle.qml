// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "LifeCycleTest"
    visible: true

    property int spyTimeout: 10000 * AmTest.timeoutFactor
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
        id: objectDestroyedSpy
        target: AmTest
        signalName: "objectDestroyed"
    }

    Timer {
        id: stopTimer
        interval: 1
        onTriggered: testCase.app.stop();
    }


    function cleanup() {
        objectDestroyedSpy.clear();
        var index = AmTest.observeObjectDestroyed(app.runtime);
        if (app.runState !== Am.NotRunning) {
            ignoreWarning(new RegExp(".*was force exited.*"));
            app.stop(true /*force*/);
            tryCompare(app, "runState", Am.NotRunning, spyTimeout);
        }

        objectDestroyedSpy.wait(spyTimeout);
        compare(objectDestroyedSpy.signalArguments[0][0], index);
    }


    // Start followed by quick stop/start in single-porcess mode caused an abort in the past
    function test_fast_stop_start() {
        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        objectDestroyedSpy.clear();
        var index = AmTest.observeObjectDestroyed(app.runtime);

        app.stop();
        tryCompare(app, "runState", Am.NotRunning, spyTimeout);

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);

        objectDestroyedSpy.wait(spyTimeout);
        compare(objectDestroyedSpy.signalArguments[0][0], index);
    }

    // Quick start/stop followd by start in single-process mode caused an abort in the past
    function test_fast_start_stop() {
        app.start();
        stopTimer.start();
        tryCompare(app, "runState", Am.NotRunning, spyTimeout);

        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);
    }

    function test_restart() {
        let actual = [];
        let expected = [Am.StartingUp, Am.Running, Am.ShuttingDown, Am.NotRunning,
                        Am.StartingUp, Am.Running];

        function onRunstateChanged(id, runState) {
            actual.push(runState);

            if (runState === Am.NotRunning)
                app.start();
            if (actual.length === expected.length)
                ApplicationManager.applicationRunStateChanged.disconnect(onRunstateChanged);
        }
        ApplicationManager.applicationRunStateChanged.connect(onRunstateChanged);

        compare(app.runState, Am.NotRunning);
        app.start();
        tryCompare(app, "runState", Am.Running, spyTimeout);
        app.stop();
        tryVerify(() => { return JSON.stringify(actual) === JSON.stringify(expected); },
                  spyTimeout);
    }
}
