/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
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
