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
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QHash>

#include <QtAppManApplication/application.h>
#include <QtAppManManager/applicationmanager.h>


QT_BEGIN_NAMESPACE_AM

class ProcessMonitorPrivate;

class ProcessMonitor : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(qint64 processId READ processId NOTIFY processIdChanged)
    Q_PROPERTY(QString applicationId READ applicationId WRITE setApplicationId NOTIFY applicationIdChanged)
    Q_PROPERTY(int reportingInterval READ reportingInterval WRITE setReportingInterval NOTIFY reportingIntervalChanged)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE setMemoryReportingEnabled NOTIFY memoryReportingEnabledChanged)
    Q_PROPERTY(bool cpuLoadReportingEnabled READ isCpuLoadReportingEnabled WRITE setCpuLoadReportingEnabled NOTIFY cpuLoadReportingEnabledChanged)
    Q_PROPERTY(bool frameRateReportingEnabled READ isFrameRateReportingEnabled WRITE setFrameRateReportingEnabled NOTIFY frameRateReportingEnabledChanged)
    Q_PROPERTY(QList<QObject *> monitoredWindows READ monitoredWindows WRITE setMonitoredWindows NOTIFY monitoredWindowsChanged)

public:
    ProcessMonitor(QObject *parent = nullptr);
    ~ProcessMonitor() override;

    Q_INVOKABLE QVariantMap get(int index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCount(int count);
    int count() const;

    qint64 processId() const;

    QString applicationId() const;
    void setApplicationId(const QString &appId);

    void setReportingInterval(int intervalInMSec);
    int reportingInterval() const;

    bool isMemoryReportingEnabled() const;
    void setMemoryReportingEnabled(bool enabled);

    bool isCpuLoadReportingEnabled() const;
    void setCpuLoadReportingEnabled(bool enabled);

    bool isFrameRateReportingEnabled() const;
    void setFrameRateReportingEnabled(bool enabled);

    QList<QObject *> monitoredWindows() const;
    void setMonitoredWindows(QList<QObject *> windows);

signals:
    void countChanged(int count);
    void processIdChanged(qint64 processId);
    void applicationIdChanged(const QString &applicationId);
    void reportingIntervalChanged(int reportingInterval);

    void memoryReportingEnabledChanged();
    void cpuLoadReportingEnabledChanged();
    void frameRateReportingEnabledChanged();

    void memoryReportingChanged(const QVariantMap &memoryVirtual, const QVariantMap &memoryRss,
                                                                  const QVariantMap &memoryPss);
    void cpuLoadReportingChanged(qreal load);
    void frameRateReportingChanged(QVariantList frameRate);
    void monitoredWindowsChanged();

private:
    ProcessMonitorPrivate *d_ptr;
    Q_DECLARE_PRIVATE(ProcessMonitor)
};

QT_END_NAMESPACE_AM
