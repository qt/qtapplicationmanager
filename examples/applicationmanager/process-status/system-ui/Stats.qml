// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtApplicationManager.SystemUI

/*
 This file shows how to use ProcessStatus alongside a Timer (instead of putting it inside a MonitorModel)
 when all that is needed is the latest information on a given application process.
 */
Grid {
    id: root
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
