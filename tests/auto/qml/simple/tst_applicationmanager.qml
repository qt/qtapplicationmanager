// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtQuick.Window 2.0
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "ApplicationManager"

    property ApplicationObject simpleApplication
    property ApplicationObject capsApplication
    // Either appman is build in single-process mode or it was started with --force-single-process
    property bool singleProcess : Qt.application.arguments.indexOf("--force-single-process") !== -1
                                  || !AmTest.buildConfig[0].QT_FEATURES.QT_FEATURE_am_multi_process
    property WindowObject lastWindowAdded: null
    property QtObject windowHandler: QtObject {
        function windowAddedHandler(window) {
            // console.info("window " + window + " added");
            lastWindowAdded = window
        }


        function windowContentStateChangedHandler(window) {
            // console.info("window content state = " + window.contentState);
        }
    }

    ListView {
        id: listView
        model: ApplicationManager
        delegate: Item {
            property var modelData: model
        }
    }

    ApplicationModel {
        id: appModel
        sortFunction: function(la, ra) { return la.id > ra.id }
    }

    function initTestCase() {
        WindowManager.windowAdded.connect(windowHandler.windowAddedHandler)
        WindowManager.windowContentStateChanged.connect(windowHandler.windowContentStateChangedHandler)

        compare(ApplicationManager.count, 2)
        simpleApplication = ApplicationManager.application(0);
        capsApplication = ApplicationManager.application(1);
    }

    function test_properties() {
        // Only true for the dummyimports
        compare(ApplicationManager.dummy, false)
        // Disabled in the am-config.yaml
        compare(ApplicationManager.securityChecksEnabled, false)

        compare(ApplicationManager.singleProcess, singleProcess)
    }

    function test_systemProperties() {
        compare(ApplicationManager.systemProperties.ignored, undefined)
        compare(ApplicationManager.systemProperties.booleanTest, true)
        compare(ApplicationManager.systemProperties.stringTest, "pelagicore")
        compare(ApplicationManager.systemProperties.intTest, -1)
        compare(ApplicationManager.systemProperties.floatTest, .5)
        compare(ApplicationManager.systemProperties.arrayTest[0], "value1")
        compare(ApplicationManager.systemProperties.arrayTest[1], "value2")
        compare(ApplicationManager.systemProperties.mapTest["key1"], "1")
        compare(ApplicationManager.systemProperties.mapTest["key2"], "2")
        compare(ApplicationManager.systemProperties.nested.level21, 21)
        compare(ApplicationManager.systemProperties.nested.level2.level31, 31)
        compare(ApplicationManager.systemProperties.nested.level2.level32, undefined)
        compare(ApplicationManager.systemProperties.nested.level21, 21)
        compare(ApplicationManager.systemProperties.nested.level22, 22)
        compare(ApplicationManager.systemProperties.pub1, 'pub1')
        compare(ApplicationManager.systemProperties.pro1, 'pro1')
        compare(ApplicationManager.systemProperties.pubandpro, 'pro2')
        compare(ApplicationManager.systemProperties.pubandpri, 'pri3')
        compare(ApplicationManager.systemProperties.proandpri, 'pri4')
        compare(ApplicationManager.systemProperties.inall, 'private')
        compare(ApplicationManager.systemProperties.nullTest, null)
    }

    function test_package() {
        compare(PackageManager.count, 2)
        verify(simpleApplication.package !== capsApplication.package)
        compare(simpleApplication.package, PackageManager.package("tld.test.simple1"))
        compare(simpleApplication.package.id, "tld.test.simple1")
        compare(simpleApplication.package.applications.length, 1)
        compare(simpleApplication.package.applications[0], simpleApplication)
    }

    function test_application() {
        var id = simpleApplication.id;
        compare(simpleApplication.id, "tld.test.simple1")
        compare(simpleApplication.runtimeName, "qml")
        compare(simpleApplication.icon.toString(), Qt.resolvedUrl("apps/tld.test.simple1/icon.png"))
        compare(simpleApplication.documentUrl, "")
        compare(simpleApplication.builtIn, true)
        compare(simpleApplication.alias, false)
        compare(simpleApplication.nonAliased, simpleApplication)
        compare(simpleApplication.capabilities.length, 0)
        compare(simpleApplication.supportedMimeTypes.length, 2)
        compare(simpleApplication.categories.length, 0)
        //Why is runtime null ? we should document this, as this is not really clear
        compare(simpleApplication.runtime, null)
        compare(simpleApplication.lastExitCode, 0)
        compare(simpleApplication.lastExitStatus, Am.NormalExit)
        compare(simpleApplication.version, "1.0")

        // Test the name getter and verify that it's returning the same object
        compare(simpleApplication, ApplicationManager.application(id))
    }

    function test_applicationProperties() {
        compare(simpleApplication.applicationProperties.ignored, undefined)
        compare(simpleApplication.applicationProperties.pro1, "pro1")
        compare(simpleApplication.applicationProperties.proandpri, "pro2")
        compare(simpleApplication.applicationProperties.pri1, undefined)
    }

    function test_indexOfApplication() {
        // Test index of
        compare(ApplicationManager.indexOfApplication(simpleApplication.id), 0)
        compare(ApplicationManager.indexOfApplication(capsApplication.id), 1)
        compare(ApplicationManager.indexOfApplication("error"), -1)
    }

    function test_applicationIds() {
        // Test app ids
        var apps = ApplicationManager.applicationIds()
        compare(apps.length, ApplicationManager.count)
        compare(ApplicationManager.applicationIds()[0], simpleApplication.id)
        compare(ApplicationManager.applicationIds()[1], capsApplication.id)
    }

    function test_capabilities() {
        var emptyCaps = ApplicationManager.capabilities(simpleApplication.id)
        compare(emptyCaps.length, 0)
        var caps = ApplicationManager.capabilities(capsApplication.id)
        compare(caps.length, 2)
    }

    function test_modelData() {
        compare(listView.count, ApplicationManager.count)
        listView.currentIndex = 0;
        compare(listView.currentItem.modelData.application, simpleApplication)
        compare(listView.currentItem.modelData.applicationId, simpleApplication.id)
        compare(listView.currentItem.modelData.name, "Simple1")
        compare(listView.currentItem.modelData.icon.toString(), Qt.resolvedUrl(simpleApplication.icon))
        compare(listView.currentItem.modelData.runtimeName, "qml")
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        compare(listView.currentItem.modelData.isBlocked, false)
        compare(listView.currentItem.modelData.isUpdating, false)
        compare(listView.currentItem.modelData.isRemovable, false)
        compare(listView.currentItem.modelData.updateProgress, 0.0)
        verify(listView.currentItem.modelData.codeFilePath.indexOf("apps/tld.test.simple1/app1.qml") !== -1)
        compare(listView.currentItem.modelData.capabilities, simpleApplication.capabilities)
        compare(listView.currentItem.modelData.version, "1.0")
    }

    SignalSpy {
        id: appModelCountSpy
        target: appModel
        signalName: "countChanged"
    }

    function test_applicationModel() {
        compare(appModel.count, 2);
        compare(appModel.indexOfApplication(capsApplication.id), 0);
        compare(appModel.indexOfApplication(simpleApplication.id), 1);
        compare(appModel.mapToSource(0), ApplicationManager.indexOfApplication(capsApplication.id));
        compare(appModel.mapFromSource(ApplicationManager.indexOfApplication(simpleApplication.id)), 1);

        appModel.sortFunction = undefined;
        compare(appModel.indexOfApplication(simpleApplication.id),
                ApplicationManager.indexOfApplication(simpleApplication.id));
        compare(appModel.indexOfApplication(capsApplication.id),
                ApplicationManager.indexOfApplication(capsApplication.id));

        appModelCountSpy.clear();
        appModel.filterFunction = function(app) { return app.capabilities.indexOf("cameraAccess") >= 0; };
        appModelCountSpy.wait(1000 * AmTest.timeoutFactor);
        compare(appModelCountSpy.count, 1);
        compare(appModel.count, 1);
        compare(appModel.indexOfApplication(capsApplication.id), 0);
        compare(appModel.indexOfApplication(simpleApplication.id), -1);

        listView.model = appModel;
        listView.currentIndex = 0;
        compare(listView.currentItem.modelData.name, "Caps");
        listView.model = ApplicationManager;

        appModel.filterFunction = function() {};
        compare(appModel.count, 0);

        appModel.filterFunction = undefined;
        compare(appModel.count, 2);
    }

    function test_get_data() {
        return [
                    {tag: "get(row)", argument: 0 },
                    {tag: "get(id)", argument: simpleApplication.id },
                ];
    }

    function test_get(data) {
        var appData = ApplicationManager.get(data.argument);

        compare(appData.application, simpleApplication)
        compare(appData.applicationId, simpleApplication.id)
        compare(appData.name, "Simple1")
        compare(appData.icon.toString(), Qt.resolvedUrl(simpleApplication.icon))
        compare(appData.runtimeName, "qml")
        compare(appData.isRunning, false)
        compare(appData.isStartingUp, false)
        compare(appData.isShuttingDown, false)
        compare(appData.isBlocked, false)
        compare(appData.isUpdating, false)
        compare(appData.isRemovable, false)
        compare(appData.updateProgress, 0.0)
        verify(appData.codeFilePath.indexOf("apps/tld.test.simple1/app1.qml") !== -1)
        compare(appData.capabilities, simpleApplication.capabilities)
        compare(appData.version, "1.0")
    }

    function test_application_object_ownership() {
        // Check that the returned Application is not owned by javascript and deleted
        // by the garbage collector
        var app = ApplicationManager.application(0);
        var id = app.id
        app = null;
        // app gets deleted now as there is now javascript reference anymore
        gc()
        // Test that the Application in the ApplicationManager is still valid
        // by accessing one of it's properties
        compare(id, ApplicationManager.application(0).id)
    }

    SignalSpy {
        id: runStateChangedSpy
        target: ApplicationManager
        signalName: "applicationRunStateChanged"
    }

    function checkApplicationState(id, state) {
        if (runStateChangedSpy.count < 1)
            runStateChangedSpy.wait(10000 * AmTest.timeoutFactor);
        verify(runStateChangedSpy.count)
        compare(runStateChangedSpy.signalArguments[0][0], id)
        compare(runStateChangedSpy.signalArguments[0][1], state)
        compare(ApplicationManager.application(id).runState, state);
        runStateChangedSpy.clear();
    }

    function test_startAndStopApplication_data() {
        return [
                    {tag: "StartStop", appId: "tld.test.simple1", index: 0, forceKill: false,
                        exitCode: 0, exitStatus: Am.NormalExit },
                    {tag: "Debug", appId: "tld.test.simple1", index: 0, forceKill: false,
                        exitCode: 0, exitStatus: Am.NormalExit },
                    {tag: "ForceKill", appId: "tld.test.simple2", index: 1, forceKill: true,
                        exitCode: 9, exitStatus: Am.ForcedExit },
                    {tag: "AutoTerminate", appId: "tld.test.simple2", index: 1, forceKill: false,
                        exitCode: 15, exitStatus: Am.ForcedExit }
                ];
    }

    function test_startAndStopApplication(data) {
        compare(ApplicationManager.application(data.appId).runState, Am.NotRunning);

        var started = false;
        if (data.tag === "Debug") {
            if (singleProcess)
                ignoreWarning("Using debug-wrappers is not supported when the application manager is running in single-process mode.");
            started = ApplicationManager.debugApplication(data.appId, "%program% %arguments%");
            if (singleProcess) {
                verify(!started);
                return;
            }
        } else {
            started = ApplicationManager.startApplication(data.appId);
        }
        verify(started);

        checkApplicationState(data.appId, Am.StartingUp);
        listView.currentIndex = data.index;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId, Am.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        // make sure we have a window
        tryCompare(WindowManager, "count", 1)
        compare(WindowManager.windowsOfApplication(data.appId).length, 1)
        compare(WindowManager.windowsOfApplication(data.appId)[0], lastWindowAdded)

        ApplicationManager.stopApplication(data.appId, data.forceKill);

        checkApplicationState(data.appId, Am.ShuttingDown);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, true)
        checkApplicationState(data.appId, Am.NotRunning);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        compare(listView.currentItem.modelData.application.lastExitCode, data.exitCode)
        compare(listView.currentItem.modelData.application.lastExitStatus, data.exitStatus)
    }

    function test_startAndStopAllApplications_data() {
        return [
                    {tag: "StopAllApplications", appId1: "tld.test.simple1", index1: 0,
                        appId2: "tld.test.simple2", index2: 1, forceKill: false, exitCode: 0,
                        exitStatus: Am.NormalExit }
               ];
    }

    function test_startAndStopAllApplications(data) {
        compare(ApplicationManager.application(data.appId1).runState, Am.NotRunning);
        compare(ApplicationManager.application(data.appId2).runState, Am.NotRunning);

        var started = false;

        started = ApplicationManager.startApplication(data.appId1);
        verify(started);


        checkApplicationState(data.appId1, Am.StartingUp);
        listView.currentIndex = data.index1;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId1, Am.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        started = ApplicationManager.startApplication(data.appId2);
        verify(started);

        checkApplicationState(data.appId2, Am.StartingUp);
        listView.currentIndex = data.index2;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId2, Am.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        ApplicationManager.stopAllApplications(data.forceKill);

        while (runStateChangedSpy.count < 4)
            runStateChangedSpy.wait(10000 * AmTest.timeoutFactor);

        var args = runStateChangedSpy.signalArguments

        for (var i = 0; i < 4; ++i) {
            var id = args[i][0]
            var state = args[i][1]

            var atPos = id.indexOf('@')
            if (atPos >= 0)
                id = id.substring(0, atPos)

            // not perfect, but the basic signal sequence is already tested in test_startAndStopApplication
            verify(id === data.appId1 || id === data.appId2, "id = " + id)
            verify(state === Am.ShuttingDown || state === Am.NotRunning)
        }
        runStateChangedSpy.clear()
    }

    function test_errors() {
        ignoreWarning("ApplicationManager::application(index): invalid index: -1");
        verify(!ApplicationManager.application(-1));
        verify(!ApplicationManager.application("invalidApplication"));
        ignoreWarning("ApplicationManager::get(index): invalid index: -1");
        compare(ApplicationManager.get(-1), {});
        compare(ApplicationManager.get("invalidApplication"), {});
        compare(ApplicationManager.applicationRunState("invalidApplication"), Am.NotRunning);

        ignoreWarning("Cannot start application: id 'invalidApplication' is not known");
        verify(!ApplicationManager.startApplication("invalidApplication"))

        //All following tests don't work in single-process mode
        if (singleProcess)
            return;

        ignoreWarning("Tried to start application tld.test.simple1 using an invalid debug-wrapper specification:  ");
        verify(!ApplicationManager.debugApplication(simpleApplication.id, " "))

        verify(ApplicationManager.startApplication(simpleApplication.id));
        checkApplicationState(simpleApplication.id, Am.StartingUp);
        checkApplicationState(simpleApplication.id, Am.Running);
        ignoreWarning("Application tld.test.simple1 is already running - cannot start with debug-wrapper: %program% %arguments%");
        verify(!ApplicationManager.debugApplication(simpleApplication.id, "%program% %arguments%"))
        ApplicationManager.stopApplication(simpleApplication.id);
        checkApplicationState(simpleApplication.id, Am.ShuttingDown);
        checkApplicationState(simpleApplication.id, Am.NotRunning);
    }

    function test_openUrl_data() {
        return [
                    {tag: "customMimeType", url: "x-test://12345", expectedApp: simpleApplication.id },
                    {tag: "text/plain", url: "file://text-file.txt", expectedApp: simpleApplication.id }
                ];
    }

    function test_openUrl(data) {
        verify(ApplicationManager.openUrl(data.url));
        checkApplicationState(data.expectedApp, Am.StartingUp);
        checkApplicationState(data.expectedApp, Am.Running);
        ApplicationManager.stopApplication(data.expectedApp);
        checkApplicationState(data.expectedApp, Am.ShuttingDown);
        checkApplicationState(data.expectedApp, Am.NotRunning);

        Qt.openUrlExternally(data.url);
        checkApplicationState(data.expectedApp, Am.StartingUp);
        checkApplicationState(data.expectedApp, Am.Running);
        ApplicationManager.stopApplication(data.expectedApp);
        checkApplicationState(data.expectedApp, Am.ShuttingDown);
        checkApplicationState(data.expectedApp, Am.NotRunning);
    }

    property bool containerSelectionCalled: false
    property string containerSelectionAppId
    property string containerSelectionConId
    function containerSelection(appId, containerId) {
        containerSelectionCalled = true;
        containerSelectionAppId = appId;
        containerSelectionConId = containerId;

        return containerId;
    }

    function test_containerSelectionFunction() {
        if (singleProcess)
            skip("The containerSelectionFunction doesn't work in single-process mode");

        compare(ApplicationManager.containerSelectionFunction, undefined);
        ApplicationManager.containerSelectionFunction = containerSelection;
        ApplicationManager.startApplication(simpleApplication.id);
        checkApplicationState(simpleApplication.id, Am.StartingUp);
        checkApplicationState(simpleApplication.id, Am.Running);
        ApplicationManager.stopApplication(simpleApplication.id);
        checkApplicationState(simpleApplication.id, Am.ShuttingDown);
        checkApplicationState(simpleApplication.id, Am.NotRunning);
        verify(containerSelectionCalled);
        compare(containerSelectionAppId, simpleApplication.id);
        compare(containerSelectionConId, "process");
    }

    function test_applicationInterface() {
        let req = IntentClient.sendIntentRequest("applicationInterfaceProperties", simpleApplication.id, { })
        verify(req)
        tryVerify(() => { return req.succeeded })
        const ai = {
            "applicationId": simpleApplication.id,
            "applicationProperties": {
                "pri1": "pri1",
                "pro1": "pro1",
                "proandpri": "pri2"
            },
            "icon": simpleApplication.icon.toString().split('/').pop(),
            "systemProperties": {
                "inall": "protected",
                "nested": {
                    "level2": {
                        "level31": "overwritten",
                        "level32": "hidden"
                    }
                },
                "pro1": "pro1",
                "proandpri": "pro4",
                "pub1": "pub1",
                "pubandpri": "pub3",
                "pubandpro": "pro2"
            },
            "version": simpleApplication.version
        }

        compare(req.result, ai)

        // cleanup
        compare(simpleApplication.runState, Am.Running)
        ApplicationManager.stopApplication(simpleApplication.id)
        tryVerify(() => { return simpleApplication.runState === Am.NotRunning })
    }
}
