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
    import QtApplicationManager 1.0
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
    import QtApplicationManager 1.0
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
            or the application-manager has to be run as the \c root user.
    \endtable

    \sa \l{GpuStatus::update}{update()}
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

