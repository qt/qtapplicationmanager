// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSSTATUS_H
#define PROCESSSTATUS_H

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtQml/QQmlParserStatus>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>
#include <QtAppManSystemUI/amnamespace.h>

QT_BEGIN_NAMESPACE_AM

class ProcessStatusPrivate;

// It's assumed that all ProcessStatus instances are created from the same thread (most likely the main one).
class Q_APPMANSYSTEMUI_EXPORT ProcessStatus : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString applicationId READ applicationId WRITE setApplicationId NOTIFY applicationIdChanged FINAL)
    Q_PROPERTY(qint64 processId READ processId NOTIFY processIdChanged FINAL)
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged FINAL)
    Q_PROPERTY(QVariantMap memoryVirtual READ memoryVirtual NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(QVariantMap memoryRss READ memoryRss NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(QVariantMap memoryPss READ memoryPss NOTIFY memoryReportingChanged FINAL)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE setMemoryReportingEnabled  NOTIFY memoryReportingEnabledChanged)
    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    ProcessStatus(QObject *parent = nullptr);
    ~ProcessStatus() override;

    QStringList roleNames() const;

    Q_INVOKABLE void update();

    qint64 processId() const;

    QString applicationId() const;
    void setApplicationId(const QString &appId);

    qreal cpuLoad() const;
    QVariantMap memoryVirtual() const;
    QVariantMap memoryRss() const;
    QVariantMap memoryPss() const;

    bool isMemoryReportingEnabled() const;
    void setMemoryReportingEnabled(bool enabled);

    void classBegin() override;
    void componentComplete() override;

Q_SIGNALS:
    void applicationIdChanged(const QString &applicationId);
    void processIdChanged(qint64 processId);
    void cpuLoadChanged();
    void memoryReportingChanged(const QVariantMap &memoryVirtual, const QVariantMap &memoryRss,
                                                                  const QVariantMap &memoryPss);
    void memoryReportingEnabledChanged(bool enabled);

private:
    Q_DECLARE_PRIVATE(ProcessStatus)
    Q_DISABLE_COPY(ProcessStatus)
};

QT_END_NAMESPACE_AM

#endif // PROCESSSTATUS_H
