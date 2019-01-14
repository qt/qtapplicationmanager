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

import QtQuick 2.4
import QtQuick.Controls 2.4
import QtApplicationManager.SystemUI 2.0

Pane {
    width: 900
    height: appsColumn.y + appsColumn.height

    // Show a warning on non-Linux platforms
    Rectangle {
        visible: Qt.platform.os !== 'linux'
        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        width: parent.width / 2
        height: warningLabel.height
        z: 1000
        color: "red"
        border.color: "white"
        Text {
            id: warningLabel
            width: parent.width
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "Please note that this example will not show sensible data on your current " +
                  "platform (" + Qt.platform.os + "). Only Linux/multi-process will produce " +
                  "all relevant data points."
        }
    }

    // Show name, icon and ProcessStatus data for each application
    Column {
        id: appsColumn
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 2
        spacing: 10
        Repeater {
            model: ApplicationManager
            ApplicationDisplay {
                name: model.name
                application: model.application
            }
        }
    }

    // Show windows of running applications
    Column {
        id: windowsColumn
        anchors.left: appsColumn.right
        anchors.right: parent.right
        anchors.margins: 10
        Repeater {
            model: WindowManager
            WindowItem {
                width: windowsColumn.width
                height: 200
                window: model.window
            }
        }
    }
}
