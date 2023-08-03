// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtApplicationManager.SystemUI


ApplicationWindow {
    id: root
    width: 800
    height: 600
    font.pixelSize: 20

    PackageServerInterface {
        id: packageServerInterface

        property string defaultUrl: ApplicationManager.systemProperties.packageServer.url
        url: serverUrl.text
        projectId: ApplicationManager.systemProperties.packageServer.projectId
        architecture: PackageManager.architecture

        onPackagesChanged: {
            packageModel.clear()
            for (let i = 0; i < packages.length; i++) {
                let p = packages[i]
                packageModel.append({
                    id:           p.id,
                    architecture: p.architecture,
                    name:         p.names["en"],
                    description:  p.descriptions["en"],
                    version:      p.version,
                    categories:   p.categories,
                    iconUrl:      p.iconUrl
                })
            }

            stack.currentIndex = packageModel.count ? 1 : 0
        }
    }

    AcknowledgeDialog {
        id: acknowledgeDialog
    }

    component TaskButton: Button {
        width: 150
        height: 100
        display: AbstractButton.TextUnderIcon
        icon.width: 40
        icon.height: 40
        font.pixelSize: 20
    }

    // Show application names and icons
    Column {
        width: 200
        Repeater {
            model: ApplicationManager
            Column {
                id: delegate

                required property bool isRunning
                required property var icon
                required property var application
                required property string name

                TaskButton {
                    icon.source: delegate.icon
                    icon.color: "transparent"
                    text: delegate.name
                    checkable: true
                    checked: delegate.isRunning

                    onToggled: checked ? delegate.application.start()
                                       : delegate.application.stop()
                }
            }
        }
    }

    // Show windows
    Column {
        anchors.right: parent.right
        Repeater {
            model: WindowManager
            WindowItem {
                required property var model
                width: 600
                height: 200
                window: model.window
            }
        }
    }

    TaskButton {
        anchors.bottom: parent.bottom

        icon.source: "package"
        text: "Packages"

        onClicked: { storeDialog.open() }
    }

    Dialog {
        id: storeDialog
        title: "Package-Server"
        standardButtons: Dialog.Close
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent.width * 3 / 4
        height: parent.height * 7 / 8
        padding: 20

        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Label { text: "Acknowledge" }
                ComboBox {
                    Layout.fillWidth: true
                    model: [
                        { value: AcknowledgeDialog.Always,           text: 'Always' },
                        { value: AcknowledgeDialog.Never,            text: 'Never' },
                        { value: AcknowledgeDialog.CapabilitiesOnly, text: 'Only for Capabilities' }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    onActivated: acknowledgeDialog.mode = currentValue
                    Component.onCompleted: currentIndex = indexOfValue(acknowledgeDialog.mode)
                }
            }
            RowLayout {
                Label { text: "Server" }
                TextField {
                    id: serverUrl
                    Layout.fillWidth: true
                    text: packageServerInterface.defaultUrl
                }
                ToolButton {
                    icon.source: "reload"
                    onClicked: packageServerInterface.reload()
                }
            }
            StackLayout {
                id: stack

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                    wrapMode: Text.Wrap
                    text: packageServerInterface.statusText
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth

                    ListView {
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        model: ListModel {
                            id: packageModel
                            dynamicRoles: true
                        }

                        delegate: ItemDelegate {
                            required property string id
                            required property var name
                            required property string iconUrl

                            property bool isInstalled: false

                            Component.onCompleted: {
                                isInstalled = (PackageManager.indexOfPackage(id) >= 0)
                            }

                            Connections {
                                target: PackageManager
                                function onPackageAdded(pkgId) {
                                    if (pkgId === id)
                                        isInstalled = true
                                }
                                function onPackageAboutToBeRemoved(pkgId) {
                                    if (pkgId === id)
                                        isInstalled = false
                                }
                            }

                            Image {
                                source: isInstalled ? "uninstall" : "install"
                                width: height
                                height: parent.height / 2
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: parent.height / 8
                            }

                            width: ListView.view.width
                            text: "<b>" + name + "</b> (" + id + ")"
                                  + "<br>&&nbsp;&&nbsp;&&nbsp;<i>[" + (isInstalled ? "currently installed"
                                                                                   : "not installed") + "]</i>"
                            icon.source: packageServerInterface.url + "/" + iconUrl
                            icon.color: "transparent"

                            onClicked: {
                                if (isInstalled)
                                    packageServerInterface.remove(id)
                                else
                                    packageServerInterface.install(id)
                            }
                        }
                    }
                }
            }
        }
    }
}
