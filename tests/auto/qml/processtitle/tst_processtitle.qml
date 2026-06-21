// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2020 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.15
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test


TestCase {
    id: testCase
    when: windowShown
    name: "ProcessTitle"
    visible: true

    property int spyTimeout: 5000 * AmTest.timeoutFactor
    property int sysuiPid

    ProcessStatus {
        id: processStatus
        applicationId: ""
        Component.onCompleted: testCase.sysuiPid = processId;
    }


    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function test_launcher_qml_data() {
        return [ { tag: "small", appId: "test.processtitle.app" },
                 { tag: "large", appId: "appappapp1appappapp2appappapp3appappapp4appappapp5appappapp6appappapp7" } ];
    }

    function test_launcher_qml(data) {
        const executable = "appman-launcher-qml";
        var sigIdx;
        var quickArg;
        var pid
        if (ApplicationManager.systemProperties.quickLaunch) {
            sigIdx = 0;
            quickArg = ": [quicklaunch]"
            tryVerify(function() {
                let out = AmTest.runProgram([ "ps", "--ppid", sysuiPid, "-o", "pid,args", "--no-headers"]).stdout
                const re = new RegExp(" *(\\d*) .*" + executable + quickArg.replace(/[\[\]:]/g, '\\$&'))
                let match = re.exec(out)
                pid = match ? match[1] : 0
                return pid
            }, spyTimeout);

            // the process sets its title asynchronously after starting, so poll for it rather
            // than waiting a fixed amount of time
            let cmdLine
            tryVerify(function() {
                cmdLine = AmTest.runProgram([ "cat", `/proc/${pid}/cmdline` ]).stdout.split('\0')[0]
                return cmdLine.endsWith(executable + quickArg)
            }, spyTimeout, "cmdLine does not end with: '" + executable + quickArg + "'");
        } else {
            sigIdx = 1;
            quickArg = ""
        }

        runStateChangedSpy.clear();
        verify(ApplicationManager.startApplication(data.appId));
        runStateChangedSpy.wait(spyTimeout);
        if (sigIdx === 1)
            runStateChangedSpy.wait(spyTimeout);

        compare(runStateChangedSpy.signalArguments[sigIdx][0], data.appId);
        compare(runStateChangedSpy.signalArguments[sigIdx][1], Am.Running);

        processStatus.applicationId = data.appId;
        pid = processStatus.processId;

        // the process sets its title asynchronously after reaching Running, so poll the title in
        // both /proc/<pid>/cmdline and ps output rather than waiting a fixed amount of time
        let checkStr = executable + ": " + data.appId
        let cmdLine
        let psOutput
        tryVerify(function() {
            cmdLine = AmTest.runProgram([ "cat", `/proc/${pid}/cmdline` ]).stdout.split('\0')[0]
            psOutput = AmTest.runProgram([ "ps", "--no-headers", pid ]).stdout.trim()
            return psOutput.endsWith(checkStr) && cmdLine.endsWith(checkStr)
        }, spyTimeout, "ps output and/or cmdline do not end with: '" + checkStr + "'");

        let environment = AmTest.runProgram([ "cat", `/proc/${pid}/environ` ]).stdout
        verify(environment.includes("AM_CONFIG=%YAML"));
        verify(environment.includes("WAYLAND_DISPLAY="));

        runStateChangedSpy.clear();
        ApplicationManager.stopAllApplications();
        runStateChangedSpy.wait(spyTimeout);
        runStateChangedSpy.wait(spyTimeout);
        compare(runStateChangedSpy.signalArguments[1][1], Am.NotRunning);
    }
}
