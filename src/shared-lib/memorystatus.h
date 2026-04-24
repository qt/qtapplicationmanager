// Copyright (C) 2026 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef MEMORYSTATUS_H
#define MEMORYSTATUS_H

#include <QtCore/QObject>

#include <QtAppManShared/qtappmansharedglobal.h>

QT_BEGIN_NAMESPACE_AM

class MemoryStatusPrivate;

class Q_APPMANSHARED_EXPORT MemoryStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint64 totalMemory READ totalMemory CONSTANT FINAL)
    Q_PROPERTY(quint64 memoryUsed READ memoryUsed NOTIFY memoryUsedChanged FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    MemoryStatus(QObject *parent = nullptr);

    quint64 totalMemory() const;
    quint64 memoryUsed() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void memoryUsedChanged();

private:
    Q_DECLARE_PRIVATE(MemoryStatus)
    Q_DISABLE_COPY(MemoryStatus)
};

QT_END_NAMESPACE_AM

#endif // MEMORYSTATUS_H
