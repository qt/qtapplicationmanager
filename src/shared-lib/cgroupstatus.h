// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CGROUPSTATUS_H
#define CGROUPSTATUS_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>
#include <QtAppManShared/pressurestallinformation.h>

QT_BEGIN_NAMESPACE_AM

class CGroupStatusPrivate;

class Q_APPMANSHARED_EXPORT CGroupStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged FINAL)

    Q_PROPERTY(quint64 memoryHigh READ memoryHigh NOTIFY pathChanged FINAL)
    Q_PROPERTY(quint64 memoryMax READ memoryMax NOTIFY pathChanged FINAL)
    Q_PROPERTY(quint64 memoryUsed READ memoryUsed NOTIFY memoryUsedChanged FINAL)
    Q_PROPERTY(quint64 memoryAnon READ memoryAnon NOTIFY memoryAnonChanged FINAL)
    Q_PROPERTY(quint64 memoryShmem READ memoryShmem NOTIFY memoryShmemChanged FINAL)

    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged FINAL)

    Q_PROPERTY(PressureStallInformation *cpuPSI READ cpuPSI CONSTANT FINAL)
    Q_PROPERTY(PressureStallInformation *memoryPSI READ memoryPSI CONSTANT FINAL)
    Q_PROPERTY(PressureStallInformation *ioPSI READ ioPSI CONSTANT FINAL)

    // returns std::limits<quint64>::max() to express cgroup v2's "max" value
    Q_PROPERTY(quint64 max READ maxValue CONSTANT FINAL)

public:
    CGroupStatus(QObject *parent = nullptr);

    QString path() const;
    void setPath(const QString &groupPath);
    quint64 memoryHigh() const;
    quint64 memoryMax() const;
    quint64 memoryUsed() const;
    quint64 memoryAnon() const;
    quint64 memoryShmem() const;

    qreal cpuLoad() const;

    PressureStallInformation *cpuPSI() const;
    PressureStallInformation *memoryPSI() const;
    PressureStallInformation *ioPSI() const;

    quint64 maxValue() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void memoryUsedChanged();
    void memoryAnonChanged();
    void memoryShmemChanged();
    void cpuLoadChanged();
    void pathChanged();

private:
    Q_DECLARE_PRIVATE(CGroupStatus)
    Q_DISABLE_COPY(CGroupStatus)
};

QT_END_NAMESPACE_AM

#endif // CGROUPSTATUS_H
