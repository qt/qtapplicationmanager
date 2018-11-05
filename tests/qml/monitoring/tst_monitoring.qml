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

import QtQuick 2.6
import QtTest 1.0
import QtApplicationManager.SystemUI 1.0
import QtApplicationManager 1.0

TestCase {
    id: testCase
    when: windowShown
    name: "Monitoring"

    SystemMonitor {
        id: systemMonitor
    }

    ListView {
        id: listView
        model: systemMonitor
        delegate: Item {}
    }

    SignalSpy {
        id: memoryReportingChangedSpy
        target: systemMonitor
        signalName: "memoryReportingChanged"
    }

    function test_mem_reporting() {
        compare(systemMonitor.count, 10);
        verify(!systemMonitor.memoryReportingEnabled);

        systemMonitor.reportingInterval = 80
        systemMonitor.count = 5;
        systemMonitor.memoryReportingEnabled = true;

        compare(systemMonitor.reportingInterval, 80);
        compare(systemMonitor.count, 5);
        verify(systemMonitor.memoryReportingEnabled);

        if (memoryReportingChangedSpy.count < 1)
            memoryReportingChangedSpy.wait(1000 * AmTest.timeoutFactor);
        compare(memoryReportingChangedSpy.count, 1);
        console.info("Used Memory: " + (memoryReportingChangedSpy.signalArguments[0][0]/1e9).toFixed(1) + " GB");
        var mem = [memoryReportingChangedSpy.signalArguments[0][0]];
        verify(mem[0] > 16000);
        compare(listView.model.get(0).memoryUsed, mem[0]);

        if (memoryReportingChangedSpy.count < 2)
            memoryReportingChangedSpy.wait(1000 * AmTest.timeoutFactor);
        compare(memoryReportingChangedSpy.count, 2);
        mem[1] = memoryReportingChangedSpy.signalArguments[1][0];
        verify(mem[1] > 16000);
        compare(listView.model.get(0).memoryUsed, mem[1]);
        compare(listView.model.get(1).memoryUsed, mem[0]);

        systemMonitor.count = 4;
        compare(listView.model.get(0).memoryUsed, mem[1]);
        compare(listView.model.get(1).memoryUsed, mem[0]);

        systemMonitor.reportingInterval = 85;
        compare(listView.model.get(0).memoryUsed, 0);
        compare(listView.model.get(1).memoryUsed, 0);
    }

    function test_static() {
        console.info("CPU Cores: " + systemMonitor.cpuCores);
        console.info("Total Memory: " + (systemMonitor.totalMemory/1e9).toFixed(1) + " GB");
        verify(systemMonitor.cpuCores >= 1);
        verify(systemMonitor.totalMemory > 64000);
        compare(systemMonitor.idleLoadThreshold, 0.1);
    }
}
