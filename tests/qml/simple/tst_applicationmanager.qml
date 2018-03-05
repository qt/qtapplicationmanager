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
import QtQuick.Window 2.0
import QtTest 1.0
import QtApplicationManager 1.0
import QtApplicationManager 1.0 as AppMan

TestCase {
    id: testCase
    when: windowShown
    name: "ApplicationManager"

    property var simpleApplication
    property var applicationAlias
    property var capsApplication
    // Either appman is build in single-process mode or it was started with --force-single-process
    property bool singleProcess : Qt.application.arguments.indexOf("--force-single-process") !== -1 || buildConfig[0].CONFIG.indexOf("multi-process") === -1
    property QtObject windowHandler: QtObject {
        function windowReadyHandler(index, window) {
            console.info("window " + index + " ready")
            window.parent = testCase
        }

        function windowClosingHandler(index, window) {
            console.info("window " + index + " closing")
        }

        function windowLostHandler(index, window) {
            console.info("window " + index + " lost")
            WindowManager.releaseWindow(window);
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
        //Wait for the debugging wrappers to be setup.
        wait(2000);
        WindowManager.windowReady.connect(windowHandler.windowReadyHandler)
        WindowManager.windowClosing.connect(windowHandler.windowClosingHandler)
        WindowManager.windowLost.connect(windowHandler.windowLostHandler)

        compare(ApplicationManager.count, 3)
        simpleApplication = ApplicationManager.application(0);
        applicationAlias = ApplicationManager.application(1);
        capsApplication = ApplicationManager.application(2);
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

    function test_application() {
        var id = simpleApplication.id;
        compare(simpleApplication.id, "tld.test.simple1")
        compare(simpleApplication.runtimeName, "qml")
        compare(simpleApplication.icon, Qt.resolvedUrl("apps/tld.test.simple1/icon.png"))
        compare(simpleApplication.documentUrl, "")
        compare(simpleApplication.importance, 0)
        compare(simpleApplication.builtIn, true)
        compare(simpleApplication.alias, false)
        compare(simpleApplication.preload, false)
        compare(simpleApplication.nonAliased, null)
        compare(simpleApplication.capabilities.length, 0)
        compare(simpleApplication.supportedMimeTypes.length, 2)
        compare(simpleApplication.categories.length, 0)
        //Why is runtime null ? we should document this, as this is not really clear
        compare(simpleApplication.runtime, null)
        compare(simpleApplication.lastExitCode, 0)
        compare(simpleApplication.lastExitStatus, AppMan.Application.NormalExit)
        compare(simpleApplication.type, AppMan.Application.Gui)
        compare(simpleApplication.backgroundMode, AppMan.Application.Auto)
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

    function test_aliasApplication() {
        // Test that the alias has the same info, be indentified as an alias and points to the original app
        compare(applicationAlias.id, "tld.test.simple1@alias")
        compare(applicationAlias.alias, true)
        compare(applicationAlias.nonAliased, simpleApplication)
        //TODO this should be a Url instead, as this is what's used in QML usually
        compare(applicationAlias.icon, Qt.resolvedUrl("apps/tld.test.simple1/icon2.png"))
        compare(applicationAlias.documentUrl, "x-test:alias")
        compare(applicationAlias.runtimeName, simpleApplication.runtimeName)
        compare(applicationAlias.importance, simpleApplication.importance)
        compare(applicationAlias.preload, simpleApplication.preload)
        compare(applicationAlias.capabilities, simpleApplication.capabilities)
        compare(applicationAlias.supportedMimeTypes, simpleApplication.supportedMimeTypes)
    }

    function test_indexOfApplication() {
        // Test index of
        compare(ApplicationManager.indexOfApplication(simpleApplication.id), 0)
        compare(ApplicationManager.indexOfApplication(applicationAlias.id), 1)
        compare(ApplicationManager.indexOfApplication("error"), -1)
    }

    function test_applicationIds() {
        // Test app ids
        var apps = ApplicationManager.applicationIds()
        compare(apps.length, ApplicationManager.count)
        compare(ApplicationManager.applicationIds()[0], simpleApplication.id)
        compare(ApplicationManager.applicationIds()[1], applicationAlias.id)
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
        compare(listView.currentItem.modelData.icon, Qt.resolvedUrl(simpleApplication.icon))
        compare(listView.currentItem.modelData.runtimeName, "qml")
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        compare(listView.currentItem.modelData.isBlocked, false)
        compare(listView.currentItem.modelData.isUpdating, false)
        compare(listView.currentItem.modelData.isRemovable, false)
        compare(listView.currentItem.modelData.updateProgress, 0.0)
        verify(listView.currentItem.modelData.codeFilePath.indexOf("apps/tld.test.simple1/app1.qml") !== -1)
        compare(listView.currentItem.modelData.backgroundMode, "Auto")
        compare(listView.currentItem.modelData.capabilities, simpleApplication.capabilities)
        compare(listView.currentItem.modelData.importance, simpleApplication.importance)
        compare(listView.currentItem.modelData.preload, simpleApplication.preload)
        compare(listView.currentItem.modelData.version, "1.0")
    }

    SignalSpy {
        id: appModelCountSpy
        target: appModel
        signalName: "countChanged"
    }

    function test_applicationModel() {
        compare(appModel.count, 3);
        compare(appModel.indexOfApplication(capsApplication.id), 0);
        compare(appModel.indexOfApplication(applicationAlias.id), 1);
        compare(appModel.indexOfApplication(simpleApplication.id), 2);
        compare(appModel.mapToSource(0), ApplicationManager.indexOfApplication(capsApplication.id));
        compare(appModel.mapFromSource(ApplicationManager.indexOfApplication(simpleApplication.id)), 2);

        appModel.sortFunction = undefined;
        compare(appModel.indexOfApplication(simpleApplication.id),
                ApplicationManager.indexOfApplication(simpleApplication.id));
        compare(appModel.indexOfApplication(applicationAlias.id),
                ApplicationManager.indexOfApplication(applicationAlias.id));
        compare(appModel.indexOfApplication(capsApplication.id),
                ApplicationManager.indexOfApplication(capsApplication.id));

        appModelCountSpy.clear();
        appModel.filterFunction = function(app) { return app.capabilities.indexOf("cameraAccess") >= 0; };
        appModelCountSpy.wait(1000);
        compare(appModelCountSpy.count, 1);
        compare(appModel.count, 1);
        compare(appModel.indexOfApplication(capsApplication.id), 0);
        compare(appModel.indexOfApplication(applicationAlias.id), -1);
        compare(appModel.indexOfApplication(simpleApplication.id), -1);

        listView.model = appModel;
        listView.currentIndex = 0;
        compare(listView.currentItem.modelData.name, "Caps");
        listView.model = ApplicationManager;

        var criteria = false;
        appModel.filterFunction = function(app) { return app.alias === criteria; };
        appModelCountSpy.wait(1000);
        compare(appModelCountSpy.count, 2);
        compare(appModel.count, 2);
        compare(appModel.indexOfApplication(applicationAlias.id), -1);
        criteria = true;
        appModel.invalidate();
        compare(appModel.count, 1);

        appModel.filterFunction = function() {};
        compare(appModel.count, 0);

        appModel.filterFunction = undefined;
        compare(appModel.count, 3);
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
        compare(appData.icon, Qt.resolvedUrl(simpleApplication.icon))
        compare(appData.runtimeName, "qml")
        compare(appData.isRunning, false)
        compare(appData.isStartingUp, false)
        compare(appData.isShuttingDown, false)
        compare(appData.isBlocked, false)
        compare(appData.isUpdating, false)
        compare(appData.isRemovable, false)
        compare(appData.updateProgress, 0.0)
        verify(appData.codeFilePath.indexOf("apps/tld.test.simple1/app1.qml") !== -1)
        compare(appData.backgroundMode, "Auto")
        compare(appData.capabilities, simpleApplication.capabilities)
        compare(appData.importance, simpleApplication.importance)
        compare(appData.preload, simpleApplication.preload)
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
            runStateChangedSpy.wait(10000);
        verify(runStateChangedSpy.count)
        compare(runStateChangedSpy.signalArguments[0][0], id)
        compare(runStateChangedSpy.signalArguments[0][1],  state)
        compare(ApplicationManager.application(id).runState, state);
        runStateChangedSpy.clear();
    }

    function test_startAndStopApplication_data() {
        return [
                    {tag: "StartStop", appId: "tld.test.simple1", index: 0, forceKill: false, exitCode: 0, exitStatus: AppMan.Application.NormalExit },
                    {tag: "StartStopAlias", appId: "tld.test.simple1@alias", index: 0, forceKill: false, exitCode: 0, exitStatus: AppMan.Application.NormalExit },
                    {tag: "Debug", appId: "tld.test.simple1", index: 0, forceKill: false, exitCode: 0, exitStatus: AppMan.Application.NormalExit },
                    {tag: "ForceKill", appId: "tld.test.simple2", index: 2, forceKill: true, exitCode: Qt.platform.os !== 'windows' ? 9 : 0, exitStatus: Qt.platform.os !== 'windows' ? AppMan.Application.ForcedExit : AppMan.Application.CrashExit },
                    {tag: "AutoTerminate", appId: "tld.test.simple2", index: 2, forceKill: false, exitCode: Qt.platform.os !== 'windows' ? 15 : 0, exitStatus: Qt.platform.os !== 'windows' ? AppMan.Application.ForcedExit : AppMan.Application.CrashExit }
                ];
    }

    function test_startAndStopApplication(data) {
        compare(ApplicationManager.application(data.appId).runState, AppMan.Application.NotRunning);

        var started = false;
        if (data.tag === "Debug") {
            if (singleProcess)
                ignoreWarning("Using debug-wrappers is not supported when the application-manager is running in single-process mode.");
            started = ApplicationManager.debugApplication(data.appId, "%program% %arguments%");
            if (singleProcess) {
                verify(!started);
                return;
            }
        } else {
            started = ApplicationManager.startApplication(data.appId);
        }
        verify(started);

        checkApplicationState(data.appId, AppMan.Application.StartingUp);
        listView.currentIndex = data.index;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId, AppMan.Application.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        ApplicationManager.stopApplication(data.appId, data.forceKill);

        checkApplicationState(data.appId, AppMan.Application.ShuttingDown);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, true)
        checkApplicationState(data.appId, AppMan.Application.NotRunning);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        compare(listView.currentItem.modelData.application.lastExitCode, data.exitCode)
        compare(listView.currentItem.modelData.application.lastExitStatus, data.exitStatus)
    }

    function test_startAndStopAllApplications_data() {
        return [
                    {tag: "StopAllApplications", appId1: "tld.test.simple1", index1: 0, appId2: "tld.test.simple2", index2: 2, forceKill: false, exitCode: 0, exitStatus: AppMan.Application.NormalExit }
               ];
    }

    function test_startAndStopAllApplications(data) {
        compare(ApplicationManager.application(data.appId1).runState, AppMan.Application.NotRunning);
        compare(ApplicationManager.application(data.appId2).runState, AppMan.Application.NotRunning);

        var started = false;

        started = ApplicationManager.startApplication(data.appId1);
        verify(started);


        checkApplicationState(data.appId1, AppMan.Application.StartingUp);
        listView.currentIndex = data.index1;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId1, AppMan.Application.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        started = ApplicationManager.startApplication(data.appId2);
        verify(started);

        checkApplicationState(data.appId2, AppMan.Application.StartingUp);
        listView.currentIndex = data.index2;
        compare(listView.currentItem.modelData.isStartingUp, true)
        compare(listView.currentItem.modelData.isRunning, false)
        compare(listView.currentItem.modelData.isShuttingDown, false)
        checkApplicationState(data.appId2, AppMan.Application.Running);
        compare(listView.currentItem.modelData.isStartingUp, false)
        compare(listView.currentItem.modelData.isRunning, true)
        compare(listView.currentItem.modelData.isShuttingDown, false)

        ApplicationManager.stopAllApplications(data.forceKill);

        while (runStateChangedSpy.count < 6)
            runStateChangedSpy.wait(10000);

        var args = runStateChangedSpy.signalArguments

        for (var i = 0; i < 6; ++i) {
            var id = args[i][0]
            var state = args[i][1]

            var atPos = id.indexOf('@')
            if (atPos >= 0)
                id = id.substring(0, atPos)

            // not perfect, but the basic signal sequence is already tested in test_startAndStopApplication
            verify(id === data.appId1 || id === data.appId2, "id = " + id)
            verify(state === AppMan.Application.ShuttingDown || state === AppMan.Application.NotRunning)
        }
        runStateChangedSpy.clear()
    }

    function test_errors() {
        ignoreWarning("invalid index: -1");
        verify(!ApplicationManager.application(-1));

        ignoreWarning("invalid index: -1");
        verify(!ApplicationManager.application("invalidApplication"));

        ignoreWarning("invalid index: -1");
        compare(ApplicationManager.get(-1), {});

        ignoreWarning("invalid index: -1");
        compare(ApplicationManager.get("invalidApplication"), {});

        ignoreWarning("invalid index: -1");
        compare(ApplicationManager.applicationRunState("invalidApplication"), AppMan.Application.NotRunning);

        ignoreWarning("Cannot start an invalid application");
        verify(!ApplicationManager.startApplication("invalidApplication"))

        //All following tests don't work in single-process mode
        if (singleProcess)
            return;

        ignoreWarning("Tried to start application tld.test.simple1 using an invalid debug-wrapper specification:  ");
        verify(!ApplicationManager.debugApplication(simpleApplication.id, " "))

        verify(ApplicationManager.startApplication(simpleApplication.id));
        checkApplicationState(simpleApplication.id, AppMan.Application.StartingUp);
        checkApplicationState(simpleApplication.id, AppMan.Application.Running);
        ignoreWarning("Application tld.test.simple1 is already running - cannot start with debug-wrapper: %program% %arguments%");
        verify(!ApplicationManager.debugApplication(simpleApplication.id, "%program% %arguments%"))
        ApplicationManager.stopApplication(simpleApplication.id, true);
        checkApplicationState(simpleApplication.id, AppMan.Application.ShuttingDown);
        checkApplicationState(simpleApplication.id, AppMan.Application.NotRunning);
    }

    function test_openUrl_data() {
        return [
                    {tag: "customMimeType", url: "x-test://12345", expectedApp: simpleApplication.id },
                    {tag: "openAlias", url: "x-test:alias", expectedApp: applicationAlias.id },
                    {tag: "text/plain", url: "file://text-file.txt", expectedApp: simpleApplication.id }
                ];
    }

    function test_openUrl(data) {
        verify(ApplicationManager.openUrl(data.url));
        checkApplicationState(data.expectedApp, AppMan.Application.StartingUp);
        checkApplicationState(data.expectedApp, AppMan.Application.Running);
        ApplicationManager.stopApplication(data.expectedApp, true);
        checkApplicationState(data.expectedApp, AppMan.Application.ShuttingDown);
        checkApplicationState(data.expectedApp, AppMan.Application.NotRunning);

        Qt.openUrlExternally(data.url);
        checkApplicationState(data.expectedApp, AppMan.Application.StartingUp);
        checkApplicationState(data.expectedApp, AppMan.Application.Running);
        ApplicationManager.stopApplication(data.expectedApp, true);
        checkApplicationState(data.expectedApp, AppMan.Application.ShuttingDown);
        checkApplicationState(data.expectedApp, AppMan.Application.NotRunning);
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
        checkApplicationState(simpleApplication.id, AppMan.Application.StartingUp);
        checkApplicationState(simpleApplication.id, AppMan.Application.Running);
        ApplicationManager.stopApplication(simpleApplication.id, true);
        checkApplicationState(simpleApplication.id, AppMan.Application.ShuttingDown);
        checkApplicationState(simpleApplication.id, AppMan.Application.NotRunning);
        verify(containerSelectionCalled);
        compare(containerSelectionAppId, simpleApplication.id);
        compare(containerSelectionConId, "process");
    }
}
