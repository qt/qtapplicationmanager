// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "cpustatus.h"

#include <QThread>

using namespace Qt::StringLiterals;

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
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        CpuStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick
    import QtApplicationManager
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
    return { u"cpuLoad"_s };
}

#include "moc_cpustatus.cpp"
