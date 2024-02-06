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
            id: dialog
            required property string taskId
            required property string packageName
            required property list<string> capabilities
            property bool acknowledged: false

            text: "Install <b>" + packageName + "</b>?"
            informativeText: capabilities.length ? "This package requests the following capabilities: " + capabilities.join(", ")
                                                 : "This package does not request any capabilities."
            buttons: QD.MessageDialog.Yes | QD.MessageDialog.No

            onAccepted: if (!acknowledged) PackageManager.acknowledgePackageInstallation(taskId)
            onRejected: if (!acknowledged) PackageManager.cancelTask(taskId)
            onVisibleChanged: if (!visible) { destroy() }

            Connections {
                target: PackageManager
                function onTaskStateChanged(taskId, state) {
                    // if somebody else (e.g. appman-controller) acknowledged already, just close
                    if (visible && (taskId === dialog.taskId)
                            && (state !== PackageManager.AwaitingAcknowledge)) {
                        acknowledged = true
                        close()
                    }
                }
            }
        }
    }
}
