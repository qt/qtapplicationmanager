/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0

TestCase {
    name: "Installer"
    when: windowShown

    property var stateList: []
    property int spyTimeout: 5000 * AmTest.timeoutFactor

    SignalSpy {
        id: taskFinishedSpy
        target: PackageManager
        signalName: "taskFinished"
    }

    SignalSpy {
        id: taskFailedSpy
        target: PackageManager
        signalName: "taskFailed"
    }

    SignalSpy {
        id: taskStateChangedSpy
        target: PackageManager
        signalName: "taskStateChanged"
    }

    SignalSpy {
        id: taskRequestingInstallationAcknowledgeSpy
        target: PackageManager
        signalName: "taskRequestingInstallationAcknowledge"
    }

    SignalSpy {
        id: applicationChangedSpy
        target: ApplicationManager
        signalName: "applicationChanged"
    }


    function init() {
        // Remove previous installations

        for (var pkg of [ "hello-world.red", "com.pelagicore.test" ]) {
            var po = PackageManager.package(pkg)
            if (!po || (po.builtIn && !po.builtInHasRemovableUpdate))
                continue
            if (PackageManager.removePackage(pkg, false, true)) {
                taskFinishedSpy.wait(spyTimeout);
                compare(taskFinishedSpy.count, 1);
                taskFinishedSpy.clear();
            }
        }
    }

    function test_1states() {
        PackageManager.packageAdded.connect(function(pkgId) {
            var pkg = PackageManager.package(pkgId);
            stateList.push(pkg.state)
            pkg.stateChanged.connect(function(state) {
                compare(state, pkg.state)
                stateList.push(state)
            })
        })

        taskStateChangedSpy.clear();
        var id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                         + "/packages/test-dev-signed.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        var pkgId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        if (!taskFinishedSpy.count)
            taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList.length, 2);
        compare(stateList[0], PackageObject.BeingInstalled)
        compare(stateList[1], PackageObject.Installed)
        stateList = []

        compare(PackageManager.package(pkgId).version, "1.0");

        id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                     + "/packages/test-update-dev-signed.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList[0], PackageObject.BeingUpdated)
        compare(stateList[1], PackageObject.Installed)
        stateList = []

        compare(PackageManager.package(pkgId).version, "2.0");

        id = PackageManager.removePackage(pkgId, false, false);

        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList[0], PackageObject.BeingRemoved)
        stateList = []
        // Cannot compare app.state any more, since app might already be dead

        verify(taskStateChangedSpy.count > 10);
        var taskStates = [ PackageManager.Executing,
                           PackageManager.AwaitingAcknowledge,
                           PackageManager.Installing,
                           PackageManager.CleaningUp,
                           PackageManager.Finished,
                           PackageManager.Executing,
                           PackageManager.AwaitingAcknowledge,
                           PackageManager.Installing,
                           PackageManager.CleaningUp,
                           PackageManager.Finished,
                           PackageManager.Executing ]
        for (var i = 0; i < taskStates.length; i++)
            compare(taskStateChangedSpy.signalArguments[i][1], taskStates[i], "- index: " + i);
    }

    function test_2cancel_update() {
        var id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                         + "/packages/test-dev-signed.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        var pkgId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        compare(pkgId, "com.pelagicore.test");
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        taskFinishedSpy.clear();

        var pkg = PackageManager.package(pkgId);
        compare(pkg.version, "1.0");

        id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                     + "/packages/test-update-dev-signed.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        pkgId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        compare(pkgId, "com.pelagicore.test");
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.cancelTask(id);

        taskFailedSpy.wait(spyTimeout);
        taskFailedSpy.clear();

        compare(pkg.version, "1.0");
    }

    function test_3cancel_builtin_update() {
        taskStateChangedSpy.clear()
        var pkg = PackageManager.package("hello-world.red");
        verify(pkg.builtIn);
        compare(pkg.icon.toString().slice(-9), "icon1.png")
        compare(pkg.version, "v1");

        var id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                     + "/packages/hello-world.red.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.cancelTask(id);

        taskFailedSpy.wait(spyTimeout);
        taskFailedSpy.clear();

        verify(pkg.builtIn);
        compare(pkg.icon.toString().slice(-9), "icon1.png")
        compare(pkg.version, "v1");
    }

    function test_4builtin_update_downgrade() {
        taskStateChangedSpy.clear()

        var id = PackageManager.startPackageInstallation(ApplicationManager.systemProperties.AM_TESTDATA_DIR
                                                     + "/packages/hello-world.red.appkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        var pkg = PackageManager.package("hello-world.red")
        compare(pkg.version, "red");
        taskFinishedSpy.clear();
        applicationChangedSpy.clear();

        // remvove is a downgrade
        verify(pkg.builtIn)
        verify(pkg.builtInHasRemovableUpdate)
        verify(PackageManager.removePackage("hello-world.red", false, true));
        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(applicationChangedSpy.count, 3);
        compare(applicationChangedSpy.signalArguments[0][0], "hello-world.red");
        compare(applicationChangedSpy.signalArguments[0][1], ["isBlocked"]);
        compare(applicationChangedSpy.signalArguments[2][1], []);

        verify(!pkg.blocked)
        compare(pkg.version, "v1");
    }
}
