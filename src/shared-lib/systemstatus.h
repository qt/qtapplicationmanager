// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SYSTEMSTATUS_H
#define SYSTEMSTATUS_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>
#include <QtAppManShared/pressurestallinformation.h>

QT_BEGIN_NAMESPACE_AM

class SystemStatusPrivate;

class Q_APPMANSHARED_EXPORT SystemStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint64 memoryMax READ memoryMax CONSTANT FINAL)
    Q_PROPERTY(quint64 memoryUsed READ memoryUsed NOTIFY memoryUsedChanged FINAL)

    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged FINAL)
    Q_PROPERTY(int cpuCores READ cpuCores CONSTANT FINAL)

    Q_PROPERTY(PressureStallInformation *cpuPSI READ cpuPSI CONSTANT FINAL)
    Q_PROPERTY(PressureStallInformation *memoryPSI READ memoryPSI CONSTANT FINAL)
    Q_PROPERTY(PressureStallInformation *ioPSI READ ioPSI CONSTANT FINAL)

public:
    SystemStatus(QObject *parent = nullptr);

    quint64 memoryMax() const;
    quint64 memoryUsed() const;

    qreal cpuLoad() const;
    int cpuCores() const;

    PressureStallInformation *cpuPSI() const;
    PressureStallInformation *memoryPSI() const;
    PressureStallInformation *ioPSI() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void memoryUsedChanged();
    void cpuLoadChanged();

private:
    Q_DECLARE_PRIVATE(SystemStatus)
    Q_DISABLE_COPY(SystemStatus)
};

QT_END_NAMESPACE_AM

#endif // SYSTEMSTATUS_H
