/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include "gpustatus.h"

/*!
    \qmltype GpuStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the GPU status.

    GpuStatus provides information on the status of the GPU. Its property values
    are updated whenever the method update() is called.

    You can use it alongside a Timer for instance to periodically query the status of the GPU:

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    GpuStatus { id: gpuStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: gpuStatus.update()
    }
    Text {
        property string loadPercent: Number(gpuStatus.gpuLoad * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "GPU load: " + loadPercent + "%"
    }
    \endqml

    You can also use this component as a MonitorModel data source if you want to plot its
    previous values over time:

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    MonitorModel {
        GpuStatus {}
    }
    \endqml

*/

QT_USE_NAMESPACE_AM

GpuStatus::GpuStatus(QObject *parent)
    : QObject(parent)
    , m_gpuReader(new GpuReader)
    , m_gpuLoad(0)
{
    m_gpuReader->setActive(true);
}

GpuStatus::~GpuStatus()
{
    m_gpuReader->setActive(false);
}

/*!
    \qmlproperty real GpuStatus::gpuLoad
    \readonly

    GPU utilization when update() was last called, as a value ranging from 0 (inclusive,
    completely idle) to 1 (inclusive, fully busy).

    \note This is dependent on tools from the graphics hardware vendor and might not work on
          every system.

    Currently, this only works on \e Linux with either \e Intel or \e NVIDIA chipsets, plus the
    tools from the respective vendors have to be installed:

    \table
    \header
        \li Hardware
        \li Tool
        \li Notes
    \row
        \li NVIDIA
        \li \c nvidia-smi
        \li The utilization will only be shown for the first GPU of the system, in case multiple GPUs
            are installed.
    \row
        \li Intel
        \li \c intel_gpu_top
        \li The binary has to be made set-UID root, e.g. via \c{sudo chmod +s $(which intel_gpu_top)},
            or the application manager has to be run as the \c root user.
    \endtable

    \sa update
*/
qreal GpuStatus::gpuLoad() const
{
    return m_gpuLoad;
}

/*!
    \qmlmethod GpuStatus::update

    Updates the gpuLoad property.

    \sa gpuLoad
*/
void GpuStatus::update()
{
    qreal newLoad = m_gpuReader->readLoadValue();
    if (newLoad != m_gpuLoad) {
        m_gpuLoad = newLoad;
        emit gpuLoadChanged();
    }
}

/*!
    \qmlproperty list<string> GpuStatus::roleNames
    \readonly

    Names of the roles provided by GpuStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList GpuStatus::roleNames() const
{
    return { qSL("gpuLoad") };
}


#include "moc_gpustatus.cpp"
