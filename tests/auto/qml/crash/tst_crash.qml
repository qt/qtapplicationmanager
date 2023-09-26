// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

TestCase {
    id: testCase
    when: windowShown
    name: "Crashtest"

    property int spyTimeout: 3000 * AmTest.timeoutFactor
    property string appId: "tld.test.crash"
    property var app: ApplicationManager.application(appId);

    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function initTestCase() {
        compare(app.lastExitStatus, ApplicationObject.NormalExit)
    }

    function test_crash_data() {
        return [ { tag: "gracefully" },
                 { tag: "illegalMemory" },
                 { tag: "illegalMemoryInThread" },
                 { tag: "unhandledException" },
                 { tag: "abort" } ];
               //{ tag: "stackOverflow" },
               //{ tag: "divideByZero" },
               //{ tag: "raise" } ];
    }

    function test_crash(data) {
        ApplicationManager.startApplication(appId);
        runStateChangedSpy.wait(spyTimeout);
        runStateChangedSpy.wait(spyTimeout);
        compare(app.runState, ApplicationObject.Running);
        ApplicationManager.startApplication(appId, data.tag);
        runStateChangedSpy.wait(spyTimeout * 5); // libbacktrace can be slow
        compare(app.runState, ApplicationObject.NotRunning);
        if (data.tag === "gracefully") {
            compare(app.lastExitStatus, ApplicationObject.NormalExit);
            compare(app.lastExitCode, 5);
        } else {
            compare(app.lastExitStatus, ApplicationObject.CrashExit);
            console.info("================================");
            console.info("=== INTENDED CRASH (TESTING) ===");
            console.info("================================");
        }
    }
}
