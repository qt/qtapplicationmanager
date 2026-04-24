// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <QHash>
#include <QElapsedTimer>
#include <private/qobject_p.h>

#include "logging.h"
#include "iostatus.h"
#include "utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

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
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        IoStatus {
            deviceNames: ["sda", "sdb"]
        }
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current value.

    \qml
    import QtQuick
    import QtApplicationManager
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

class IoReader;

class IoStatusPrivate : public QObjectPrivate
{
public:
    void addIoReader(const QString &deviceName);

    QStringList m_deviceNames;
    QHash<QString, IoReader *> m_ioHash;
    QVariantMap m_ioLoad;
};

class IoReader
{
public:
    QElapsedTimer m_lastCheck;
    qint64 m_lastIoTime = 0;
    qreal m_load = 0;
    QFile m_statFile;

    IoReader(const QString &device)
    {
#if defined(Q_OS_LINUX)
        m_statFile.setFileName(testRootPathPrefix() + u"/sys/block/"_s + device + u"/stat");
        if (!m_statFile.open(QIODevice::ReadOnly))
            qCWarning(LogSystem) << "Cannot not read I/O statistics from" << m_statFile.fileName();
        else
            readLoadValue(); // prime the m_last* values
#else
        Q_UNUSED(device);
#endif
    }

    void readLoadValue()
    {
#if defined(Q_OS_LINUX)
        if (!m_statFile.isOpen())
            return;

        m_statFile.seek(0);
        const QByteArray buffer = m_statFile.readAll();

        qsizetype pos = 0;
        QVector<qint64> values;

        while (pos < buffer.size() && values.size() < 11) {
            if (!::isdigit(buffer.at(pos))) {
                ++pos;
                continue;
            }

            char *endPtr = nullptr;
            qint64 val = ::strtoll(buffer.constData() + pos,
                                   &endPtr,
                                   10); // check missing for over-/underflow
            values << val;
            pos = int(endPtr - buffer.constData() + 1);
        }

        qint64 elapsed;
        if (m_lastCheck.isValid()) {
            elapsed = m_lastCheck.restart();
        } else {
            elapsed = -1;
            m_lastCheck.start();
            return;
        }

        if (elapsed == 0) // this should never happen but just in case
            return;

        if (values.size() >= 11) {
            qint64 ioTime = values.at(9);

            m_load = qreal(ioTime - m_lastIoTime) / qreal(elapsed);
            m_lastIoTime = ioTime;
        } else {
            m_load = 0;
        }
#endif
    }
};

IoStatus::IoStatus(QObject *parent)
    : QObject(*new IoStatusPrivate, parent)
{ }

IoStatus::~IoStatus()
{
    Q_D(IoStatus);
    qDeleteAll(d->m_ioHash);
}

/*!
    \qmlproperty list<string> IoStatus::deviceNames

    Names of the I/O devices to be probed.

    \note Currently this is only supported on Linux: device names have to match to filenames in
    the \c /sys/block directory.
*/
QStringList IoStatus::deviceNames() const
{
    Q_D(const IoStatus);
    return d->m_deviceNames;
}

void IoStatus::setDeviceNames(const QStringList &value)
{
    Q_D(IoStatus);
    qDeleteAll(d->m_ioHash);
    d->m_ioHash.clear();

    d->m_deviceNames = value;

    for (const auto &deviceName : std::as_const(d->m_deviceNames))
        d->addIoReader(deviceName);

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
    Q_D(const IoStatus);
    return d->m_ioLoad;
}

/*!
    \qmlproperty list<string> IoStatus::roleNames
    \readonly

    Names of the roles provided by IoStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList IoStatus::roleNames() const
{
    return { u"ioLoad"_s };
}

/*!
    \qmlmethod void IoStatus::update()

    Updates the ioLoad property.

    \sa ioLoad
*/
void IoStatus::update()
{
    Q_D(IoStatus);
    d->m_ioLoad.clear();
    for (auto it = d->m_ioHash.cbegin(); it != d->m_ioHash.cend(); ++it) {
        it.value()->readLoadValue();
        d->m_ioLoad.insert(it.key(), it.value()->m_load);
    }
    emit ioLoadChanged();
}

void IoStatusPrivate::addIoReader(const QString &deviceName)
{
#if defined(Q_OS_LINUX)
    if (!QFile::exists(testRootPathPrefix() + u"/dev/" + deviceName))
        return;
#endif
    if (m_ioHash.contains(deviceName))
        return;

    auto *ior = new IoReader(deviceName);
    m_ioHash.insert(deviceName, ior);
}

QT_END_NAMESPACE_AM

#include "moc_iostatus.cpp"
