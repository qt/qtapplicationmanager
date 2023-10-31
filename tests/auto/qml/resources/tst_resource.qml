// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.11
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test
import widgets 1.0

TestCase {
    id: testCase
    when: windowShown
    name: "ResourceTest"

    RedRect {}
    MagentaRect {}

    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    SignalSpy {
        id: windowPropertyChangedSpy
        target: WindowManager
        signalName: "windowPropertyChanged"
    }

    function test_basic_data() {
        return [ { tag: "app1" },
                 { tag: "app2" } ];
    }

    function test_basic(data) {
        wait(1200 * AmTest.timeoutFactor);    // wait for quicklaunch

        var app = ApplicationManager.application(data.tag);
        windowPropertyChangedSpy.clear();

        app.start();
        while (app.runState !== ApplicationObject.Running)
            runStateChangedSpy.wait(3000 * AmTest.timeoutFactor);

        if (data.tag === "app2") {
            windowPropertyChangedSpy.wait(2000 * AmTest.timeoutFactor);
            compare(windowPropertyChangedSpy.count, 1);
            compare(windowPropertyChangedSpy.signalArguments[0][0].windowProperty("meaning"), 42);
        }

        app.stop();
        while (app.runState !== ApplicationObject.NotRunning)
            runStateChangedSpy.wait(3000 * AmTest.timeoutFactor);
    }
}
