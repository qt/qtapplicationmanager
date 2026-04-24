// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CGROUPSTATUS_H
#define CGROUPSTATUS_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>

QT_BEGIN_NAMESPACE_AM

class CGroupStatus;
class CGroupStatusPrivate;
class PressureStallInformationPrivate;

class Q_APPMANSHARED_EXPORT PressureStallInformation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged FINAL)
    Q_PROPERTY(Type type READ type CONSTANT FINAL)
    Q_PROPERTY(quint64 timeWindow READ timeWindow WRITE setTimeWindow NOTIFY timeWindowChanged FINAL)
    Q_PROPERTY(quint64 stallTime READ stallTime WRITE setStallTime NOTIFY stallTimeChanged FINAL)

public:
    enum class Mode : uint { Off, Some, Full };
    Q_ENUM(Mode)

    enum class Type : uint { Cpu, Memory, Io };
    Q_ENUM(Type)

    Mode mode() const;
    void setMode(Mode mode);

    Type type() const;

    quint64 timeWindow() const;
    void setTimeWindow(quint64 timeWindow);

    quint64 stallTime() const;
    void setStallTime(quint64 stallTime);

Q_SIGNALS:
    void modeChanged();
    void timeWindowChanged();
    void stallTimeChanged();

    void triggered();

private:
    PressureStallInformation(CGroupStatus *cgroupStatus, Type type);

    Q_DECLARE_PRIVATE(PressureStallInformation)
    Q_DISABLE_COPY(PressureStallInformation)

    friend class CGroupStatus;
};

class Q_APPMANSHARED_EXPORT CGroupStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged FINAL)

    Q_PROPERTY(quint64 memoryHigh READ memoryHigh NOTIFY pathChanged FINAL)
    Q_PROPERTY(quint64 memoryMax READ memoryMax NOTIFY pathChanged FINAL)
    Q_PROPERTY(quint64 memoryUsed READ memoryUsed NOTIFY memoryUsedChanged FINAL)

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

    PressureStallInformation *cpuPSI() const;
    PressureStallInformation *memoryPSI() const;
    PressureStallInformation *ioPSI() const;

    quint64 maxValue() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void memoryUsedChanged();
    void pathChanged();

private:
    Q_DECLARE_PRIVATE(CGroupStatus)
    Q_DISABLE_COPY(CGroupStatus)
};

QT_END_NAMESPACE_AM

#endif // CGROUPSTATUS_H
