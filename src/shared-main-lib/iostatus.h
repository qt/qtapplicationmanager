// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <QtAppManMonitor/systemreader.h>

QT_BEGIN_NAMESPACE_AM

class IoStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/IoStatus 2.0")
    Q_PROPERTY(QStringList deviceNames READ deviceNames WRITE setDeviceNames NOTIFY deviceNamesChanged)
    Q_PROPERTY(QVariantMap ioLoad READ ioLoad NOTIFY ioLoadChanged)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT)

public:
    IoStatus(QObject *parent = nullptr);
    virtual ~IoStatus();

    QStringList deviceNames() const;
    void setDeviceNames(const QStringList &value);

    QVariantMap ioLoad() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

signals:
    void deviceNamesChanged();
    void ioLoadChanged();

private:
    void addIoReader(const QString &deviceName);

    QStringList m_deviceNames;
    QHash<QString, IoReader *> m_ioHash;
    QVariantMap m_ioLoad;
};

QT_END_NAMESPACE_AM

