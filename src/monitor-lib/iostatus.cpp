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

#include "iostatus.h"

#include <QFile>

/*!
    \qmltype IoStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the status of I/O devices.

    IoStatus provides information on the status of I/O devices.
    Its property values are updated whenever the method update() is called.

    You can use this component as a MonitorModel data source if you want to plot its
    previous values over time.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    MonitorModel {
        IoStatus {
            deviceNames: ["sda", "sdb"]
        }
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    IoStatus {
        id: ioStatus
        deviceNames: ["sda", "sdb"]
    }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: ioStatus.update()
    }
    Text {
        property string loadPercent: Number(ioStatus.ioLoad.sda * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "sda load: " + loadPercent + "%"
    }
    \endqml
*/

QT_USE_NAMESPACE_AM

IoStatus::IoStatus(QObject *parent)
    : QObject(parent)
{
}

IoStatus::~IoStatus()
{
    qDeleteAll(m_ioHash);
}

/*!
    \qmlproperty list<string> IoStatus::deviceNames

    Names of the I/O devices to be probed.

    \note Currently this is only supported on Linux: device names have to match to filenames in
    the \c /sys/block directory.
*/
QStringList IoStatus::deviceNames() const
{
    return m_deviceNames;
}

void IoStatus::setDeviceNames(const QStringList &value)
{
    qDeleteAll(m_ioHash);
    m_ioHash.clear();

    m_deviceNames = value;

    for (auto it = m_deviceNames.cbegin(); it != m_deviceNames.cend(); ++it)
        addIoReader(*it);

    emit deviceNamesChanged();
}

/*!
    \qmlproperty var IoStatus::ioLoad
    \readonly

    A map of devices registered in deviceNames and their corresponding I/O loads in the
    range [0, 1]. For instance the load of a device named "sda" can be accessed through
    \c ioLoad.sda.

    Devices whose status could not be fetched won't be present in this property.

    The value of this property is updated when update() is called.

    \sa update
*/
QVariantMap IoStatus::ioLoad() const
{
    return m_ioLoad;
}

/*!
    \qmlproperty list<string> IoStatus::roleNames
    \readonly

    Names of the roles provided by IoStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList IoStatus::roleNames() const
{
    return { qSL("ioLoad") };
}

/*!
    \qmlmethod IoStatus::update

    Updates the ioLoad property.

    \sa ioLoad
*/
void IoStatus::update()
{
    m_ioLoad.clear();
    for (auto it = m_ioHash.cbegin(); it != m_ioHash.cend(); ++it) {
        qreal ioVal = it.value()->readLoadValue();
        m_ioLoad.insert(it.key(), ioVal);
    }
    emit ioLoadChanged();
}

void IoStatus::addIoReader(const QString &deviceName)
{
    if (!QFile::exists(qSL("/dev/") + deviceName))
        return;
    if (m_ioHash.contains(deviceName))
        return;

    IoReader *ior = new IoReader(deviceName.toLocal8Bit().constData());
    m_ioHash.insert(deviceName, ior);
}
