// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef IOSTATUS_H
#define IOSTATUS_H

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
    Q_PROPERTY(QStringList deviceNames READ deviceNames WRITE setDeviceNames NOTIFY deviceNamesChanged FINAL)
    Q_PROPERTY(QVariantMap ioLoad READ ioLoad NOTIFY ioLoadChanged FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    IoStatus(QObject *parent = nullptr);
    ~IoStatus() override;

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


#endif // IOSTATUS_H
