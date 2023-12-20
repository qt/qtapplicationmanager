// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "memorystatus.h"

using namespace Qt::StringLiterals;

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
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        MemoryStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick
    import QtApplicationManager
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
    return { u"memoryUsed"_s };
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

#include "moc_memorystatus.cpp"
