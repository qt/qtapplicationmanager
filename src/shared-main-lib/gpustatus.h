// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef GPUSTATUS_H
#define GPUSTATUS_H

#include <memory>

#include <QtCore/QObject>

#include <QtAppManCommon/global.h>
#include <QtAppManMonitor/systemreader.h>

QT_BEGIN_NAMESPACE_AM

class GpuStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal gpuLoad READ gpuLoad NOTIFY gpuLoadChanged FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    GpuStatus(QObject *parent = nullptr);
    ~GpuStatus() override;

    qreal gpuLoad() const;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

Q_SIGNALS:
    void gpuLoadChanged();

private:
    std::unique_ptr<GpuReader> m_gpuReader;
    qreal m_gpuLoad;
};

QT_END_NAMESPACE_AM

#endif // GPUSTATUS_H
