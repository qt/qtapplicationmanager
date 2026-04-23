// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef IOSTATUS_H
#define IOSTATUS_H

#include <QtAppManShared/qtappmansharedglobal.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>


QT_BEGIN_NAMESPACE_AM

class IoStatusPrivate;

class Q_APPMANSHARED_EXPORT IoStatus : public QObject
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

Q_SIGNALS:
    void deviceNamesChanged();
    void ioLoadChanged();

private:
    Q_DECLARE_PRIVATE(IoStatus)
    Q_DISABLE_COPY(IoStatus)
};

QT_END_NAMESPACE_AM


#endif // IOSTATUS_H
