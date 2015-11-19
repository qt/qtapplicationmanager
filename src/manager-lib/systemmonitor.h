/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QAbstractListModel>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

class SystemMonitorPrivate;

class SystemMonitor : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int reportingInterval READ reportingInterval WRITE setReportingInterval)
    Q_PROPERTY(int reportingRange READ reportingRange WRITE setReportingRange)
    Q_PROPERTY(qreal idleLoadAverage READ idleLoadAverage WRITE setIdleLoadAverage)
    Q_PROPERTY(int totalMemory READ totalMemory CONSTANT)
    Q_PROPERTY(int cpuCores READ cpuCores CONSTANT)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE enableMemoryReporting)
    Q_PROPERTY(bool cpuLoadReportingEnabled READ isCpuLoadReportingEnabled WRITE enableCpuLoadReporting)

public:
    ~SystemMonitor();
    static SystemMonitor *createInstance();
    static SystemMonitor *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    int count() const { return rowCount(); }
    Q_INVOKABLE QVariantMap get(int index) const;

    quint64 totalMemory() const;
    int cpuCores() const;

    void setIdleLoadAverage(qreal loadAverage);
    qreal idleLoadAverage() const;

    Q_INVOKABLE bool setMemoryWarningThresholds(qreal lowWarning, qreal criticalWarning);
    Q_INVOKABLE qreal memoryLowWarningThreshold() const;
    Q_INVOKABLE qreal memoryCriticalWarningThreshold() const;

    void enableMemoryReporting(bool enabled);
    bool isMemoryReportingEnabled() const;

    void enableCpuLoadReporting(bool enabled);
    bool isCpuLoadReportingEnabled() const;

    Q_INVOKABLE bool addIoLoadReporting(const QString &deviceName);
    Q_INVOKABLE void removeIoLoadReporting(const QString &deviceName);
    Q_INVOKABLE QStringList ioLoadReportingDevices() const;

    void setReportingInterval(int intervalInMSec);
    int reportingInterval() const;

    void setReportingRange(int rangeInMSec);
    int reportingRange() const;

signals:
    void countChanged();
    void idle();
    void memoryLowWarning();
    void memoryCriticalWarning();

    void memoryReportingChanged(quint64 total, quint64 used);
    void cpuLoadReportingChanged(int interval, qreal load);
    void ioLoadReportingChanged(const QString &device, int interval, qreal load);

private:
    SystemMonitor();

    static SystemMonitor *s_instance;

    SystemMonitorPrivate *d_ptr;
    Q_DECLARE_PRIVATE(SystemMonitor)
};
