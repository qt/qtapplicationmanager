// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <cstdio>
#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QPair>
#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class StartupTimer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/StartupTimer 2.0 SINGLETON")

    Q_PROPERTY(quint64 timeToFirstFrame READ timeToFirstFrame NOTIFY timeToFirstFrameChanged FINAL)
    Q_PROPERTY(quint64 systemUpTime READ systemUpTime NOTIFY systemUpTimeChanged FINAL)
    Q_PROPERTY(bool automaticReporting READ automaticReporting WRITE setAutomaticReporting NOTIFY automaticReportingChanged FINAL)

public:
    static StartupTimer *instance();
    ~StartupTimer() override;

    Q_INVOKABLE void checkpoint(const QString &name);
    Q_INVOKABLE void createReport(const QString &title = QString());

    quint64 timeToFirstFrame() const;
    quint64 systemUpTime() const;
    bool automaticReporting() const;

    void checkpoint(const char *name);
    void createAutomaticReport(const QString &title);
    void checkFirstFrame();
    void reset();

    void setAutomaticReporting(bool enableAutomaticReporting);

signals:
    void timeToFirstFrameChanged(quint64 timeToFirstFrame);
    void systemUpTimeChanged(quint64 systemUpTime);
    void automaticReportingChanged(bool setAutomaticReporting);

private:
    StartupTimer();
    static StartupTimer *s_instance;

    static QByteArray formatMicroSecs(quint64 micros);

    FILE *m_output = nullptr;
    bool m_initialized = false;
    bool m_automaticReporting = true;
    quint64 m_processCreation = 0;
    quint64 m_timeToFirstFrame = 0;
    quint64 m_systemUpTime = 0;
    QElapsedTimer m_timer;
    QVector<std::pair<quint64, QByteArray>> m_checkpoints;

    Q_DISABLE_COPY_MOVE(StartupTimer)
};

QT_END_NAMESPACE_AM
