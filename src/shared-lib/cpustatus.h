// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CPUSTATUS_H
#define CPUSTATUS_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>

QT_BEGIN_NAMESPACE_AM

class CpuStatusPrivate;

class Q_APPMANSHARED_EXPORT CpuStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged FINAL)
    Q_PROPERTY(int cpuCores READ cpuCores CONSTANT FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    CpuStatus(QObject *parent = nullptr);

    qreal cpuLoad() const;
    int cpuCores() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void cpuLoadChanged();

private:
    Q_DECLARE_PRIVATE(CpuStatus)
    Q_DISABLE_COPY(CpuStatus)
};

QT_END_NAMESPACE_AM

#endif // CPUSTATUS_H
