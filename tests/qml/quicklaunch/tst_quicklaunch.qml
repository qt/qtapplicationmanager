/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
import QtApplicationManager.SystemUI 2.0

// Tests are meaningless in single-process mode, but still work

TestCase {
    id: testCase
    when: windowShown
    name: "Quicklaunch"

    property bool acknowledged: false

    SignalSpy {
        id: windowAddedSpy
        target: WindowManager
        signalName: "windowAdded"
    }

    SignalSpy {
        id: runStateChangedSpy
        signalName: "runStateChanged"
    }

    ApplicationIPCInterface {
        function acknowledge() { acknowledged = true; }
        Component.onCompleted: ApplicationIPCManager.registerInterface(this, "quicklaunch.interface", {});
    }


    function test_quicklaunch() {
        var app = ApplicationManager.application("tld.test.quicklaunch");
        runStateChangedSpy.target = app;

        wait(1000);
        // Check for quick-launching is done every second in appman. After 1s now, this test
        // sometimes caused some race where the app would not be started at all in the past:
        app.start();
        windowAddedSpy.wait(3000);
        tryCompare(testCase, "acknowledged", true);
        runStateChangedSpy.clear();
        app.stop(true);
        runStateChangedSpy.wait(3000);    // wait for ShuttingDown
        runStateChangedSpy.wait(3000);    // wait for NotRunning

        wait(1000);
        // Unfortunately there is no reliable means to determine, whether a quicklaunch process
        // is running, but after at least 2s now, there should be a process that can be attached to.
        acknowledged = false;
        app.start();
        windowAddedSpy.wait(3000);
        tryCompare(testCase, "acknowledged", true);
        runStateChangedSpy.clear();
        app.stop(true);
        runStateChangedSpy.wait(3000);    // wait for ShuttingDown
        runStateChangedSpy.wait(3000);    // wait for NotRunning
    }
}
