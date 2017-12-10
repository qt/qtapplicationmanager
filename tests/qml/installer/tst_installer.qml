/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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
import QtApplicationManager 1.0 as AM

TestCase {
    name: "Installer"
    when: windowShown

    SignalSpy {
        id: taskFinishedSpy
        target: AM.ApplicationInstaller
        signalName: "taskFinished"
    }

    SignalSpy {
        id: taskRequestingInstallationAcknowledgeSpy
        target: AM.ApplicationInstaller
        signalName: "taskRequestingInstallationAcknowledge"
    }

    SignalSpy {
        id: applicationAddedSpy
        target: AM.ApplicationManager
        signalName: "applicationAdded"
    }

    SignalSpy {
        id: stateChangedSpy
        signalName: "stateChanged"
    }

    function test_application_state() {
        // App could potentially be installed already. Remove it.
        if (AM.ApplicationInstaller.removePackage("test.install.app", false, true)) {
            taskFinishedSpy.wait(2000);
            compare(taskFinishedSpy.count, 1);
            taskFinishedSpy.clear();
        }

        var id = AM.ApplicationInstaller.startPackageInstallation("internal-0", "appv1.pkg")
        taskRequestingInstallationAcknowledgeSpy.wait(2000);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        taskRequestingInstallationAcknowledgeSpy.clear();
        AM.ApplicationInstaller.acknowledgePackageInstallation(id);

        applicationAddedSpy.wait(2000);
        var appId = applicationAddedSpy.signalArguments[0][0];
        var app = AM.ApplicationManager.application(appId);
        compare(app.state, AM.Application.BeingInstalled)
        stateChangedSpy.target = app;
        stateChangedSpy.wait(2000);
        compare(stateChangedSpy.signalArguments[0][0], AM.Application.Installed)
        compare(app.state, AM.Application.Installed)

        var id = AM.ApplicationInstaller.startPackageInstallation("internal-0", "appv2.pkg")
        taskRequestingInstallationAcknowledgeSpy.wait(2000);
        compare(taskRequestingInstallationAcknowledgeSpy.count, 1);
        compare(taskRequestingInstallationAcknowledgeSpy.signalArguments[0][0], id);
        AM.ApplicationInstaller.acknowledgePackageInstallation(id);

        stateChangedSpy.wait(2000);
        compare(stateChangedSpy.signalArguments[1][0], AM.Application.BeingUpdated)
        compare(app.state, AM.Application.BeingUpdated)
        stateChangedSpy.wait(2000);
        compare(stateChangedSpy.signalArguments[2][0], AM.Application.Installed)
        compare(app.state, AM.Application.Installed)

        id = AM.ApplicationInstaller.removePackage(appId, false, false);
        stateChangedSpy.wait(2000);
        compare(stateChangedSpy.signalArguments[3][0], AM.Application.BeingRemoved)
        // Cannot compare app.state any more, since app might already be dead
    }
}
