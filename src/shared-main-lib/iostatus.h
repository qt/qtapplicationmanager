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

#pragma once

#include <QtAppManCommon/global.h>

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariant>

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

