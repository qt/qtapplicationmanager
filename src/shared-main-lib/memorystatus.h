// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <QtAppManCommon/global.h>
#include <QtAppManMonitor/systemreader.h>

QT_BEGIN_NAMESPACE_AM

class MemoryStatus : public QObject
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

signals:
    void memoryUsedChanged();

private:
    std::unique_ptr<MemoryReader> m_memoryReader;
    quint64 m_memoryUsed;
};

QT_END_NAMESPACE_AM

