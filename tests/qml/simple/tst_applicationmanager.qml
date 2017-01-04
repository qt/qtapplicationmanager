/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
import QtApplicationManager 1.0
import QtApplicationManager 1.0 as AppMan

TestCase {
    name: "ApplicationManager"

    property var simpleApplication
    property var applicationAlias
    property var capsApplication

    ListView {
        id: listView
        model: ApplicationManager
        delegate: Item {
            property var modelData: model
        }
    }

    function initTestCase() {
        compare(ApplicationManager.count, 3)
        simpleApplication = ApplicationManager.application(0);
        applicationAlias = ApplicationManager.application(1);
        capsApplication = ApplicationManager.application(2);
    }

    function test_systemProperties() {
        function sub(obj) {
            compare(obj.ignored, undefined)
            compare(obj.booleanTest, true)
            compare(obj.stringTest, "pelagicore")
            compare(obj.intTest, -1)
            compare(obj.floatTest, .5)
            compare(obj.arrayTest[0], "value1")
            compare(obj.arrayTest[1], "value2")
            compare(obj.mapTest["key1"], "1")
            compare(obj.mapTest["key2"], "2")
            compare(obj.nested.level21, 21)
            compare(obj.nested.level2.level31, 31)
            compare(obj.nested.level2.level32, undefined)
            compare(obj.nested.level21, 21)
            compare(obj.nested.level22, 22)
            compare(obj.pub1, 'pub1')
            compare(obj.pro1, 'pro1')
            compare(obj.pubandpro, 'pro2')
            compare(obj.pubandpri, 'pri3')
            compare(obj.proandpri, 'pri4')
            compare(obj.inall, 'private')

            expectFailContinue("", "~ should report a null variant, but undefined is reported")
            compare(ApplicationManager.systemProperties.nullTest, null)
        }
        sub(ApplicationManager.systemProperties);
        sub(ApplicationManager.additionalConfiguration);
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
        compare(simpleApplication.supportedMimeTypes.length, 0)
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
        compare(applicationAlias.documentUrl, "alias")
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
        compare(listView.currentItem.modelData.isLocked, false)
        compare(listView.currentItem.modelData.isUpdating, false)
        compare(listView.currentItem.modelData.isRemovable, false)
        compare(listView.currentItem.modelData.updateProgress, 0.0)
        //TODO return URL
        compare(Qt.resolvedUrl(listView.currentItem.modelData.codeFilePath), Qt.resolvedUrl("apps/tld.test.simple1/app1.qml"))
        compare(listView.currentItem.modelData.backgroundMode, "Auto")
        compare(listView.currentItem.modelData.capabilities, simpleApplication.capabilities)
        compare(listView.currentItem.modelData.importance, simpleApplication.importance)
        compare(listView.currentItem.modelData.preload, simpleApplication.preload)
        compare(listView.currentItem.modelData.version, "1.0")
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

    //TODO add tests for:
    //Start/stop/debug application, identify, openUrl
    //Test activated
}
