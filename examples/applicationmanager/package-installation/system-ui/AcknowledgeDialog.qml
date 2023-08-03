// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQml
import QtQuick.Dialogs as QD
import QtApplicationManager.SystemUI

QtObject {
    enum AcknowledgeMode {
        Always,                // always ask the user
        Never,                 // never ask the user
        CapabilitiesOnly       // only ask the user, if the package needs capabilities
    }

    property int mode: AcknowledgeDialog.AcknowledgeMode.Always

    property Connections connections: Connections {
        target: PackageManager
        function onTaskRequestingInstallationAcknowledge(taskId, pkg, extraMetaData, extraSignedMetaData) {
            // reduce the capabilities of all applications down to a set of unique values
            let capsSet = new Set()
            pkg.applications.forEach((app) => app.capabilities.forEach((cap) => capsSet.add(cap)))
            let capabilities = Array.from(capsSet)

            if ((mode === AcknowledgeDialog.Never)
                    || ((mode === AcknowledgeDialog.CapabilitiesOnly) && !capabilities.length)) {
                PackageManager.acknowledgePackageInstallation(taskId)

            } else if ((mode === AcknowledgeDialog.Always)
                    || ((mode === AcknowledgeDialog.CapabilitiesOnly) && capabilities.length)) {
                let d = acknowledgeDialog.createObject(root.contentItem, {
                                                           taskId: taskId,
                                                           packageName: pkg.name,
                                                           capabilities: capabilities
                                                       })
                d.open()
            }
        }
    }

    property Component acknowledgeDialog: Component {
        QD.MessageDialog {
            required property string taskId
            required property string packageName
            required property list<string> capabilities

            text: "Install <b>" + packageName + "</b>?"
            informativeText: capabilities.length ? "This package requests the following capabilities: " + capabilities.join(", ")
                                                 : "This package does not request any capabilities."
            buttons: QD.MessageDialog.Yes | QD.MessageDialog.No

            onAccepted: PackageManager.acknowledgePackageInstallation(taskId)
            onRejected: PackageManager.cancelTask(taskId)
            onVisibleChanged: if (!visible) { destroy() }
        }
    }
}
