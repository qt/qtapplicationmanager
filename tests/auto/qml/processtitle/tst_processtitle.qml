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
        Component.onCompleted: sysuiPid = processId;
    }


    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function test_launcher_qml_data() {
        return [ { tag: "small", appId: "test.processtitle.app", resId: "test.processtitle.app" },
                 { tag: "large", appId: "appappapp1appappapp2appappapp3appappapp4appappapp5appappapp6appappapp7",
                                 resId: "appappapp1appappapp2appappapp3appappapp4appappapp5appappapp6appa" } ];
    }

    function test_launcher_qml(data) {
        const executable = "appman-launcher-qml";
        var sigIdx;
        var quickArg;
        var pid
        if (ApplicationManager.systemProperties.quickLaunch) {
            sigIdx = 0;
            quickArg = " --quicklaunch"
            tryVerify(function() {
                let out = AmTest.runProgram([ "ps", "--ppid", sysuiPid, "-o", "pid,args", "--no-headers"]).stdout
                const re = new RegExp(" *(\\d*) .*" + executable + quickArg)
                let match = re.exec(out)
                pid = match ? match[1] : 0
                return pid
            }, spyTimeout);
            wait(250 * AmTest.timeoutFactor);

            let cmdLine = AmTest.runProgram([ "cat", `/proc/${pid}/cmdline` ]).stdout.split('\0')[0]
            if (cmdLine.includes("/qemu-"))
                skip("This doesn't work inside qemu")

            verify(cmdLine.endsWith(executable + quickArg),
                   "cmdLine: '" + cmdLine + "' does not end with: '" + executable + quickArg + "'");
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
        compare(runStateChangedSpy.signalArguments[sigIdx][1], ApplicationObject.Running);

        processStatus.applicationId = data.appId;
        pid = processStatus.processId;

        let cmdLine = AmTest.runProgram([ "cat", `/proc/${pid}/cmdline` ]).stdout.split('\0')[0]
        if (cmdLine.includes("/qemu-"))
            skip("This doesn't work inside qemu")

        let psOutput = AmTest.runProgram([ "ps", "--no-headers", pid ]).stdout.trim()
        let checkStr = executable + ": " + data.resId + quickArg
        verify(psOutput.endsWith(checkStr), "ps output: '" + psOutput + "' does not end with: '" + checkStr + "'");
        verify(cmdLine.endsWith(checkStr), "cmd.line: '" + cmdLine + "' does not end with: '" + checkStr + "'");

        let environment = AmTest.runProgram([ "cat", `/proc/${pid}/environ` ]).stdout
        verify(environment.includes("AM_CONFIG=%YAML"));
        verify(environment.includes("AM_NO_DLT_LOGGING=1"));
        verify(environment.includes("WAYLAND_DISPLAY="));

        runStateChangedSpy.clear();
        ApplicationManager.stopAllApplications();
        runStateChangedSpy.wait(spyTimeout);
        runStateChangedSpy.wait(spyTimeout);
        compare(runStateChangedSpy.signalArguments[1][1], ApplicationObject.NotRunning);
    }
}
