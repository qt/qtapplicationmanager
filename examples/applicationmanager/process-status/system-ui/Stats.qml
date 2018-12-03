/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

import QtQuick 2.4
import QtQuick.Controls 2.4
import QtApplicationManager.SystemUI 1.0

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
