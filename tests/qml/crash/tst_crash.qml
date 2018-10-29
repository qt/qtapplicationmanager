/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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
import QtApplicationManager.SystemUI 1.0

TestCase {
    id: testCase
    when: windowShown
    name: "Crashtest"

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
                 { tag: "unhandledException" } ];
               //{ tag: "stackOverflow" },
               //{ tag: "divideByZero" },
               //{ tag: "abort" },
               //{ tag: "raise" } ];
    }

    function test_crash(data) {
        if (ApplicationManager.singleProcess)
            skip("Application crash recovery not supported in single-process mode");

        ApplicationManager.startApplication(appId);
        runStateChangedSpy.wait(3000);
        runStateChangedSpy.wait(3000);
        compare(app.runState, ApplicationObject.Running);
        ApplicationManager.startApplication(appId, data.tag);
        runStateChangedSpy.wait(3000);
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
