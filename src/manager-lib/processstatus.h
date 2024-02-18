// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QAtomicInteger>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QVariant>

#include <QtAppManCommon/global.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManManager/application.h>
#include <QtAppManMonitor/processreader.h>

QT_BEGIN_NAMESPACE_AM

// It's assumed that all ProcessStatus instances are created from the same thread (most likely the main one).
class ProcessStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString applicationId READ applicationId WRITE setApplicationId NOTIFY applicationIdChanged FINAL)
    Q_PROPERTY(qint64 processId READ processId NOTIFY processIdChanged FINAL)
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged FINAL)
    Q_PROPERTY(QVariantMap memoryVirtual READ memoryVirtual NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(QVariantMap memoryRss READ memoryRss NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(QVariantMap memoryPss READ memoryPss NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE setMemoryReportingEnabled
                                           NOTIFY memoryReportingEnabledChanged)
    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)
public:
    ProcessStatus(QObject *parent = nullptr);
    ~ProcessStatus() override;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

    qint64 processId() const;

    QString applicationId() const;
    void setApplicationId(const QString &appId);

    qreal cpuLoad();
    QVariantMap memoryVirtual() const;
    QVariantMap memoryRss() const;
    QVariantMap memoryPss() const;

    bool isMemoryReportingEnabled() const;
    void setMemoryReportingEnabled(bool enabled);

signals:
    void applicationIdChanged(const QString &applicationId);
    void processIdChanged(qint64 processId);
    void cpuLoadChanged();
    void memoryReportingChanged(const QVariantMap &memoryVirtual, const QVariantMap &memoryRss,
                                                                  const QVariantMap &memoryPss);
    void memoryReportingEnabledChanged(bool enabled);

private slots:
    void onRunStateChanged(QtAM::Am::RunState state);

private:
    void fetchReadings();
    void determinePid();

    QString m_appId;
    qint64 m_pid = 0;

    qreal m_cpuLoad = 0;
    QVariantMap m_memoryVirtual;
    QVariantMap m_memoryRss;
    QVariantMap m_memoryPss;
    bool m_memoryReportingEnabled = true;

    QPointer<Application> m_application;

    bool m_pendingUpdate = false;
    ProcessReader *m_reader;
    static QThread *m_workerThread;
    static int m_instanceCount;
};

QT_END_NAMESPACE_AM
