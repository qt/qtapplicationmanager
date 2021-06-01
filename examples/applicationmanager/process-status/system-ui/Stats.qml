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

/*
 This file shows how to use ProcessStatus alongside a Timer (instead of putting it inside a MonitorModel)
 when all that is needed is the latest information on a given application process.
 */
Grid {
    spacing: 10
    columns: 2
    rows: 5

    property var application

    property var status: ProcessStatus {
        id: processStatus
        applicationId: root.application.id
    }
    property var timer: Timer {
        id: updateTimer
        interval: 500
        repeat: true
        running: root.visible && root.application.runState === Am.Running
        onTriggered: processStatus.update()
    }


    Label { text: "processId: " + processStatus.processId; font.pixelSize: 15 }
    Label {
        property string loadPercent: Number(processStatus.cpuLoad * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "cpuLoad: " + loadPercent + "%"
        font.pixelSize: 15
    }
    MemoryText { name: "PSS.total"; value: processStatus.memoryPss.total }
    MemoryText { name: "PSS.text"; value: processStatus.memoryPss.text }
    MemoryText { name: "PSS.heap"; value: processStatus.memoryPss.heap }
    MemoryText { name: "RSS.total"; value: processStatus.memoryRss.total }
    MemoryText { name: "RSS.text"; value: processStatus.memoryRss.text }
    MemoryText { name: "RSS.heap"; value: processStatus.memoryRss.heap }
    MemoryText { name: "Virtual.total"; value: processStatus.memoryVirtual.total }
}
