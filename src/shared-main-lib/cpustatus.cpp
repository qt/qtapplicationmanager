/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include "cpustatus.h"

#include <QThread>

/*!
    \qmltype CpuStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the CPU status.

    As the name implies, CpuStatus provides information on the status of the CPU. Its property values
    are updated whenever the method update() is called.

    You can use this component as a MonitorModel data source if you want to plot its
    previous values over time.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    MonitorModel {
        CpuStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    CpuStatus { id: cpuStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: cpuStatus.update()
    }
    Text {
        property string loadPercent: Number(cpuStatus.cpuLoad * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "cpuLoad: " + loadPercent + "%"
    }
    \endqml
*/

QT_USE_NAMESPACE_AM

CpuStatus::CpuStatus(QObject *parent)
    : QObject(parent)
    , m_cpuReader(new CpuReader)
    , m_cpuLoad(0)
{
}

/*!
    \qmlproperty real CpuStatus::cpuLoad
    \readonly

    Holds the overall system's CPU utilization at the point when update() was last called, as a
    value ranging from 0 (inclusive, completely idle) to 1 (inclusive, fully busy).

    \sa CpuStatus::update
*/
qreal CpuStatus::cpuLoad() const
{
    return m_cpuLoad;
}

/*!
    \qmlproperty int CpuStatus::cpuCores
    \readonly

    The number of physical CPU cores that are installed on the system.
*/
int CpuStatus::cpuCores() const
{
    return QThread::idealThreadCount();
}

/*!
    \qmlmethod CpuStatus::update

    Updates the cpuLoad property.

    \sa CpuStatus::cpuLoad
*/
void CpuStatus::update()
{
    qreal newLoad = m_cpuReader->readLoadValue();
    if (newLoad != m_cpuLoad) {
        m_cpuLoad = newLoad;
        emit cpuLoadChanged();
    }
}

/*!
    \qmlproperty list<string> CpuStatus::roleNames
    \readonly

    Names of the roles provided by CpuStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList CpuStatus::roleNames() const
{
    return { qSL("cpuLoad") };
}

#include "moc_cpustatus.cpp"
