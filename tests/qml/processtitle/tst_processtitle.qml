/****************************************************************************
**
** Copyright (C) 2020 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

import QtQuick 2.15
import QtTest 1.0
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0


TestCase {
    id: testCase
    when: windowShown
    name: "ProcessTitle"
    visible: true

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
                pid =  AmTest.findChildProcess(sysuiPid, executable + quickArg);
                return pid
            });
            wait(250);
            verify(AmTest.cmdLine(pid).endsWith(executable + quickArg));
        } else {
            sigIdx = 1;
            quickArg = ""
        }

        runStateChangedSpy.clear();
        verify(ApplicationManager.startApplication(data.appId));
        runStateChangedSpy.wait();
        if (sigIdx === 1)
            runStateChangedSpy.wait();

        compare(runStateChangedSpy.signalArguments[sigIdx][0], data.appId);
        compare(runStateChangedSpy.signalArguments[sigIdx][1], ApplicationObject.Running);

        processStatus.applicationId = data.appId;
        pid = processStatus.processId;
        verify(AmTest.ps(pid).endsWith(executable + ": " + data.resId + quickArg));
        verify(AmTest.cmdLine(pid).endsWith(executable + ": " + data.resId + quickArg));
        verify(AmTest.environment(pid).includes("AM_CONFIG=%YAML"));
        verify(AmTest.environment(pid).includes("AM_NO_DLT_LOGGING=1"));
        verify(AmTest.environment(pid).includes("WAYLAND_DISPLAY="));

        runStateChangedSpy.clear();
        ApplicationManager.stopAllApplications();
        runStateChangedSpy.wait();
        runStateChangedSpy.wait();
        compare(runStateChangedSpy.signalArguments[1][1], ApplicationObject.NotRunning);
    }
}
