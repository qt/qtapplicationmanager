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
import QtApplicationManager 2.0

TestCase {
    name: "Installer"
    when: windowShown

    SignalSpy {
        id: taskFinishedSpy
        target: ApplicationInstaller
        signalName: "taskFinished"
    }

    SignalSpy {
        id: taskStateChangedSpy
        target: ApplicationInstaller
        signalName: "taskStateChanged"
    }

    SignalSpy {
        id: taskRequestingInstallationAcknowledgeSpy
        target: ApplicationInstaller
        signalName: "taskRequestingInstallationAcknowledge"
    }

    SignalSpy {
        id: applicationAddedSpy
        target: ApplicationManager
        signalName: "applicationAdded"
    }

    property var stateList: []
    property int spyTimeout: 5000 * AmTest.timeoutFactor

    function test_states() {
        // App could potentially be installed already. Remove it.
        if (ApplicationInstaller.removePackage("test.install.app", false, true)) {
            taskFinishedSpy.wait(spyTimeout);
            compare(taskFinishedSpy.count, 1);
            taskFinishedSpy.clear();
        }

        ApplicationManager.applicationAdded.connect(function(appId) {
            var app = ApplicationManager.application(appId);
            app.stateChanged.connect(function(state) {
                compare(state, app.state)
                stateList.push(state)
            })
        })

        var id = ApplicationInstaller.startPackageInstallation("internal-0", "appv1.pkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        var appId = taskRequestingInstallationAcknowledgeSpy.signalArguments[0][1].id
        taskRequestingInstallationAcknowledgeSpy.clear();
        ApplicationInstaller.acknowledgePackageInstallation(id);

        if (!taskFinishedSpy.count)
            taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList.length, 2);
        compare(stateList[0], ApplicationObject.BeingInstalled)
        compare(stateList[1], ApplicationObject.Installed)
        stateList = []

        id = ApplicationInstaller.startPackageInstallation("internal-0", "appv2.pkg")
        taskRequestingInstallationAcknowledgeSpy.wait(spyTimeout);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        ApplicationInstaller.acknowledgePackageInstallation(id);

        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList[0], ApplicationObject.BeingUpdated)
        compare(stateList[1], ApplicationObject.Installed)
        stateList = []

        id = ApplicationInstaller.removePackage(appId, false, false);

        taskFinishedSpy.wait(spyTimeout);
        compare(taskFinishedSpy.count, 1);
        taskFinishedSpy.clear();

        compare(stateList[0], ApplicationObject.BeingRemoved)
        stateList = []
        // Cannot compare app.state any more, since app might already be dead

        verify(taskStateChangedSpy.count > 10);
        var taskStates = [ ApplicationInstaller.Executing,
                           ApplicationInstaller.AwaitingAcknowledge,
                           ApplicationInstaller.Installing,
                           ApplicationInstaller.CleaningUp,
                           ApplicationInstaller.Finished,
                           ApplicationInstaller.Executing,
                           ApplicationInstaller.AwaitingAcknowledge,
                           ApplicationInstaller.Installing,
                           ApplicationInstaller.CleaningUp,
                           ApplicationInstaller.Finished,
                           ApplicationInstaller.Executing ]
        for (var i = 0; i < taskStates.length; i++)
            compare(taskStateChangedSpy.signalArguments[i][1], taskStates[i], "- index: " + i);
    }
}
