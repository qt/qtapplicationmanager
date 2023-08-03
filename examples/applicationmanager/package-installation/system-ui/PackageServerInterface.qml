// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQml
import QtApplicationManager.SystemUI


QtObject {
    id: root
    required property string url
    required property string projectId
    required property string architecture

    property string statusText
    property bool isCompatible
    property var packages

    function reload() { _serverHello() }

    function install(id) {
        let dlurl = url + "/package/download?id=" + id + "&architecture=" + architecture

        let taskId = PackageManager.startPackageInstallation(dlurl)
        return taskId
    }

    function remove(id) {
        let taskId = PackageManager.removePackage(id, true)
        return taskId
    }

    onUrlChanged: reload()
    Component.onCompleted: reload()

    // private:

    function _serverHello() {
        packages = []
        isCompatible = false
        statusText = "Connecting..."

        let req = new XMLHttpRequest()
        req.open("GET", root.url + "/hello?project-id=" + root.projectId)
        req.onreadystatechange = function() {
            if (req.readyState === XMLHttpRequest.DONE) {
                if (req.status == 200) {
                    let result = JSON.parse(req.responseText)
                    let status = result["status"]

                    if (status === "ok") {
                        statusText = ""
                        isCompatible = true
                        _serverListPackages()
                    } else if (status === "incompatible-project-id") {
                        statusText = "Incompatible project id"
                    } else {
                        statusText = "Received invalid JSON status: " + status
                    }
                } else {
                    statusText = "Failed to connect to server"
                }
            }
        }
        req.send(null)
    }

    function _serverListPackages() {
        if (!isCompatible)
            return

        let req = new XMLHttpRequest()
        req.open("GET", root.url + "/package/list?architecture=" + root.architecture)
        req.onreadystatechange = function() {
            if (req.readyState === XMLHttpRequest.DONE && req.status == 200) {
                let ps = JSON.parse(req.responseText)
                if (Array.isArray(ps))
                    packages = ps
                else
                    statusText = "No compatible packages found on server"
            }
        }
        req.send(null)
    }
}
