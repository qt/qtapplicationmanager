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

#include "memorystatus.h"

/*!
    \qmltype MemoryStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the status of the RAM.

    MemoryStatus provides information on the status of the system's RAM (random-access memory).
    Its property values are updated whenever the method update() is called.

    You can use this component as a MonitorModel data source if you want to plot its
    previous values over time.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    MonitorModel {
        MemoryStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick 2.11
    import QtApplicationManager 2.0
    ...
    MemoryStatus { id: memoryStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: memoryStatus.update()
    }
    Text {
        text: "memory used: " + (memoryStatus.memoryUsed / 1e6).toFixed(0) + " MB"
    }
    \endqml
*/

QT_USE_NAMESPACE_AM

MemoryStatus::MemoryStatus(QObject *parent)
    : QObject(parent)
    , m_memoryReader(new MemoryReader)
    , m_memoryUsed(0)
{
}

/*!
    \qmlproperty int MemoryStatus::totalMemory
    \readonly

    The total amount of physical memory (RAM) installed on the system in bytes.

    \sa MemoryStatus::memoryUsed
*/
quint64 MemoryStatus::totalMemory() const
{
#if defined(Q_OS_LINUX)
    auto limit = m_memoryReader->groupLimit();
    if (limit > 0 && limit < m_memoryReader->totalValue())
        return limit;
    else
        return m_memoryReader->totalValue();
#else
    return m_memoryReader->totalValue();
#endif
}

/*!
    \qmlproperty int MemoryStatus::memoryUsed
    \readonly

    The amount of physical memory (RAM) used in bytes.

    The value of this property is updated when MemoryStatus::update is called.

    \sa totalMemory
*/
quint64 MemoryStatus::memoryUsed() const
{
    return m_memoryUsed;
}

/*!
    \qmlproperty list<string> MemoryStatus::roleNames
    \readonly

    Names of the roles provided by MemoryStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList MemoryStatus::roleNames() const
{
    return { qSL("memoryUsed") };
}

/*!
    \qmlmethod MemoryStatus::update

    Updates the memoryUsed property.

    \sa memoryUsed
*/
void MemoryStatus::update()
{
    quint64 newReading = m_memoryReader->readUsedValue();
    if (m_memoryUsed != newReading) {
        m_memoryUsed = newReading;
        emit memoryUsedChanged();
    }
}
