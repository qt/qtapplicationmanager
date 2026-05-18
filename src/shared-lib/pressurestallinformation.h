// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PRESSURESTALLINFORMATION_H
#define PRESSURESTALLINFORMATION_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>

QT_BEGIN_NAMESPACE_AM

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

    PressureStallInformation(Type type, QObject *parent);

    Mode mode() const;
    void setMode(Mode mode);

    Type type() const;

    quint64 timeWindow() const;
    void setTimeWindow(quint64 timeWindow);

    quint64 stallTime() const;
    void setStallTime(quint64 stallTime);

    void setPressureFile(const QString &path);

Q_SIGNALS:
    void modeChanged();
    void timeWindowChanged();
    void stallTimeChanged();

    void triggered();

private:
    Q_DECLARE_PRIVATE(PressureStallInformation)
    Q_DISABLE_COPY(PressureStallInformation)
};

QT_END_NAMESPACE_AM

#endif // PRESSURESTALLINFORMATION_H
