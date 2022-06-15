// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>

#include <QtAppManMonitor/systemreader.h>

#include <QObject>
#include <QScopedPointer>

QT_BEGIN_NAMESPACE_AM

class GpuStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/GpuStatus 2.0")
    Q_PROPERTY(qreal gpuLoad READ gpuLoad NOTIFY gpuLoadChanged)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT)

public:
    GpuStatus(QObject *parent = nullptr);

    qreal gpuLoad() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

signals:
    void gpuLoadChanged();

private:
    QScopedPointer<GpuReader> m_gpuReader;
    qreal m_gpuLoad;
};

QT_END_NAMESPACE_AM
