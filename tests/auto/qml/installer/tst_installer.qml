// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test

TestCase {
    name: "Installer"
    when: windowShown

    property string packageDir: ApplicationManager.systemProperties.AM_TESTDATA_DIR + "/packages/"

    // this should be initTestCase(), but a skip() there doesn't skip the whole TestCase the
    // same way as it works on the C++ side, so we have to call this from every test function
    function checkSkip() {
        if (!AmTest.dirExists(packageDir))
            skip("No test packages available in the data/ directory")
    }

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
        id: taskBlockingUntilInstallationAcknowledgeSpy
        target: PackageManager
        signalName: "taskBlockingUntilInstallationAcknowledge"
    }

    SignalSpy {
        id: applicationChangedSpy
        target: ApplicationManager
        signalName: "applicationChanged"
    }

    SignalSpy {
        id: applicationRunStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }


    function init() {
        // Remove previous installations

        for (var pkg of [ "other-test-pkg", "test-pkg" ]) {
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
        checkSkip()

        PackageManager.packageAdded.connect(function(pkgId) {
            var pkg = PackageManager.package(pkgId);
            stateList.push(pkg.state)
            pkg.stateChanged.connect(function(state) {
                compare(state, pkg.state)
                stateList.push(state)
            })
        })

        taskStateChangedSpy.clear();
        var id = PackageManager.startPackageInstallation("file:" + packageDir + "test-dev-signed.ampkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);

        // this Package is temporary and it will be gone, as soon as we call
        // acknowledgePackageInstallation below
        var pkg = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1]
        var pkgId = pkg.id
        compare(pkgId, "test-pkg")
        compare(pkg.applications.length, 1)
        compare(pkg.applications[0].id, "test-app")
        compare(pkg.applications[0].runtimeName, "native")

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

        id = PackageManager.startPackageInstallation("file:" + packageDir + "test-update-dev-signed.ampkg")
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
        checkSkip()

        var id = PackageManager.startPackageInstallation("file:" + packageDir + "test-dev-signed.ampkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        var pkgId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        compare(pkgId, "test-pkg");
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        taskFinishedSpy.clear();

        var pkg = PackageManager.package(pkgId);
        compare(pkg.version, "1.0");

        id = PackageManager.startPackageInstallation("file:" + packageDir + "test-update-dev-signed.ampkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        pkgId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        compare(pkgId, "test-pkg");
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.cancelTask(id);

        taskFailedSpy.wait(spyTimeout);
        taskFailedSpy.clear();

        compare(pkg.version, "1.0");
    }

    function test_3cancel_builtin_update() {
        checkSkip()

        taskStateChangedSpy.clear()
        var pkg = PackageManager.package("other-test-pkg");
        verify(pkg.builtIn);
        compare(pkg.version, "v1");
        compare(pkg.icon.toString().slice(-9), "icon1.png")

        var id = PackageManager.startPackageInstallation("file:" + packageDir + "other-test.ampkg")
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
        checkSkip()

        taskStateChangedSpy.clear()

        var pkg = PackageManager.package("other-test-pkg")

        // record where the app's files are on every state change
        var codeDirs = []
        function captureCodeDir(state) {
            var app = ApplicationManager.application("other-test-app")
            if (app)
                codeDirs.push({ state: state, codeDir: app.codeDir })
        }
        pkg.stateChanged.connect(captureCodeDir)

        var id = PackageManager.startPackageInstallation("file:" + packageDir + "other-test.ampkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskRequestingInstallationAcknowledgeSpy.clear();
        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        compare(pkg.version, "other");
        taskFinishedSpy.clear();
        applicationChangedSpy.clear();

        // remvove is a downgrade
        verify(pkg.builtIn)
        verify(pkg.builtInHasRemovableUpdate)
        verify(PackageManager.removePackage("other-test-pkg", false, true));
        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        // blocked, state -> BeingDowngraded, state -> Installed, unblocked
        compare(applicationChangedSpy.count, 4);
        compare(applicationChangedSpy.signalArguments[0][0], "other-test-app");
        compare(applicationChangedSpy.signalArguments[0][1], ["isBlocked"]);
        compare(applicationChangedSpy.signalArguments[1][1], []);
        compare(applicationChangedSpy.signalArguments[2][1], []);
        compare(applicationChangedSpy.signalArguments[3][1], ["isBlocked"]);

        pkg.stateChanged.disconnect(captureCodeDir)

        compare(codeDirs.length, 4)
        compare(codeDirs[0].state, PackageObject.BeingUpdated)
        compare(codeDirs[1].state, PackageObject.Installed)
        compare(codeDirs[2].state, PackageObject.BeingDowngraded)
        compare(codeDirs[3].state, PackageObject.Installed)

        // an update leaves the built-in content in place, so it keeps its unsuffixed directory
        verify(!codeDirs[0].codeDir.endsWith("+"))
        compare(codeDirs[0].codeDir, codeDirs[3].codeDir)
        // the installed update is renamed away while it is being downgraded
        compare(codeDirs[2].codeDir, codeDirs[1].codeDir + "-")

        verify(!pkg.blocked)
        compare(pkg.version, "v1");
    }

    function test_5stop_on_update() {
        checkSkip()

        taskStateChangedSpy.clear()
        taskBlockingUntilInstallationAcknowledgeSpy.clear()
        applicationRunStateChangedSpy.clear()

        // start the app
        var app = ApplicationManager.application("other-test-app")
        verify(app)
        verify(app.start())
        applicationRunStateChangedSpy.wait(spyTimeout);
        compare(applicationRunStateChangedSpy.count, 1);
        compare(applicationRunStateChangedSpy.signalArguments[0][0], "other-test-app")
        compare(applicationRunStateChangedSpy.signalArguments[0][1], Am.StartingUp)
        applicationRunStateChangedSpy.clear()
        applicationRunStateChangedSpy.wait(spyTimeout);
        compare(applicationRunStateChangedSpy.count, 1);
        compare(applicationRunStateChangedSpy.signalArguments[0][0], "other-test-app")
        compare(applicationRunStateChangedSpy.signalArguments[0][1], Am.Running)
        applicationRunStateChangedSpy.clear()

        // now install the update
        var id = PackageManager.startPackageInstallation("file:" + packageDir + "other-test.ampkg")
        taskBlockingUntilInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskBlockingUntilInstallationAcknowledgeSpy.count, 1);
        compare(taskBlockingUntilInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskBlockingUntilInstallationAcknowledgeSpy.clear();

        // make sure the app gets shut down during the update
        compare(applicationRunStateChangedSpy.count, 2);
        compare(applicationRunStateChangedSpy.signalArguments[0][0], "other-test-app")
        compare(applicationRunStateChangedSpy.signalArguments[0][1], Am.ShuttingDown)
        compare(applicationRunStateChangedSpy.signalArguments[1][0], "other-test-app")
        compare(applicationRunStateChangedSpy.signalArguments[1][1], Am.NotRunning)
        applicationRunStateChangedSpy.clear()

        PackageManager.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        var pkg = PackageManager.package("other-test-pkg")
        compare(pkg.version, "other");
        taskFinishedSpy.clear();
        applicationChangedSpy.clear();
    }
}
