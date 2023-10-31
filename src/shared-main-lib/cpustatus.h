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

class CpuStatus : public QObject
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

signals:
    void cpuLoadChanged();

private:
    std::unique_ptr<CpuReader> m_cpuReader;
    qreal m_cpuLoad;
};

QT_END_NAMESPACE_AM
