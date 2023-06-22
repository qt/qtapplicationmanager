// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtApplicationManager.Application
import NetworkHelper

ApplicationManagerWindow {
    Rectangle {
        anchors.fill: parent
        border.color: "red"

        ListView {
            anchors.fill: parent
            model: networkHelper.ipAddresses
            header: Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "IP ADDRESSES"
                font.bold: true
                height: 40
            }
            delegate: Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData
            }
        }

        NetworkHelper {
            id: networkHelper

            Component.onCompleted:  {
                console.log("ip", networkHelper.ipAddresses)
            }
        }
    }
}
