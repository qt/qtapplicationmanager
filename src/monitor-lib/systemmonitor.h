/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QAbstractListModel>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class SystemMonitorPrivate;
class XProcessMonitor;

class SystemMonitor : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int reportingInterval READ reportingInterval WRITE setReportingInterval NOTIFY reportingIntervalChanged)
    Q_PROPERTY(int reportingRange READ reportingRange WRITE setReportingRange NOTIFY reportingRangeChanged)  // deprecated
    Q_PROPERTY(qreal idleLoadThreshold READ idleLoadThreshold WRITE setIdleLoadThreshold NOTIFY idleLoadThresholdChanged)
    Q_PROPERTY(quint64 totalMemory READ totalMemory CONSTANT)
    Q_PROPERTY(quint64 memoryUsed READ memoryUsed NOTIFY memoryReportingChanged)
    Q_PROPERTY(int cpuCores READ cpuCores CONSTANT)
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadReportingChanged)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE setMemoryReportingEnabled NOTIFY memoryReportingEnabledChanged)
    Q_PROPERTY(bool cpuLoadReportingEnabled READ isCpuLoadReportingEnabled WRITE setCpuLoadReportingEnabled NOTIFY cpuLoadReportingEnabledChanged)
    Q_PROPERTY(bool fpsReportingEnabled READ isFpsReportingEnabled WRITE setFpsReportingEnabled NOTIFY fpsReportingEnabledChanged)
    Q_PROPERTY(bool idle READ isIdle NOTIFY idleChanged)

public:
    ~SystemMonitor();
    static SystemMonitor *createInstance();
    static SystemMonitor *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCount(int count);
    int count() const;

    Q_INVOKABLE QVariantMap get(int index) const;

    quint64 totalMemory() const;
    quint64 memoryUsed() const;
    int cpuCores() const;
    qreal cpuLoad() const;

    void setIdleLoadThreshold(qreal loadThreshold);
    qreal idleLoadThreshold() const;

    bool isIdle() const;

    Q_INVOKABLE bool setMemoryWarningThresholds(qreal lowWarning, qreal criticalWarning);
    Q_INVOKABLE qreal memoryLowWarningThreshold() const;
    Q_INVOKABLE qreal memoryCriticalWarningThreshold() const;

    void setMemoryReportingEnabled(bool enabled);
    bool isMemoryReportingEnabled() const;

    void setCpuLoadReportingEnabled(bool enabled);
    bool isCpuLoadReportingEnabled() const;

    Q_INVOKABLE bool addIoLoadReporting(const QString &deviceName);
    Q_INVOKABLE void removeIoLoadReporting(const QString &deviceName);
    Q_INVOKABLE QStringList ioLoadReportingDevices() const;

    void setFpsReportingEnabled(bool enabled);
    bool isFpsReportingEnabled() const;

    void setReportingInterval(int intervalInMSec);
    int reportingInterval() const;

    void setReportingRange(int rangeInMSec);
    int reportingRange() const;

    // semi-public API: used for the WindowManager to report FPS
    void reportFrameSwap();

    Q_INVOKABLE QObject *getProcessMonitor(const QString &appId);  // experimental only!

signals:
    void countChanged();
    void idleChanged(bool idle);
    void reportingIntervalChanged(int reportingInterval);
    void reportingRangeChanged(int reportingRange);
    void idleLoadThresholdChanged(qreal idleLoadThreshold);

    void memoryReportingChanged(quint64 used);
    void cpuLoadReportingChanged(qreal load);
    void ioLoadReportingChanged(const QString &device, qreal load);
    void fpsReportingChanged(qreal average, qreal minimum, qreal maximum, qreal jitter);

    void memoryReportingEnabledChanged();
    void cpuLoadReportingEnabledChanged();
    void fpsReportingEnabledChanged();

private:
    SystemMonitor();
    static SystemMonitor *s_instance;

    SystemMonitorPrivate *d_ptr;
    Q_DECLARE_PRIVATE(SystemMonitor)
};

QT_END_NAMESPACE_AM
