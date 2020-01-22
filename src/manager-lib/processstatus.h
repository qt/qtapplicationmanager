/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QAtomicInteger>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QVariant>

#include <QtAppManCommon/global.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManManager/application.h>
#include <QtAppManMonitor/processreader.h>

QT_BEGIN_NAMESPACE_AM

// It's assumed that all ProcessStatus instances are created from the same thread (most likely the main one).
class ProcessStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/ProcessStatus 2.0")
    Q_PROPERTY(QString applicationId READ applicationId WRITE setApplicationId NOTIFY applicationIdChanged)
    Q_PROPERTY(qint64 processId READ processId NOTIFY processIdChanged)
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY cpuLoadChanged)
    Q_PROPERTY(QVariantMap memoryVirtual READ memoryVirtual NOTIFY memoryReportingChanged)
    Q_PROPERTY(QVariantMap memoryRss READ memoryRss NOTIFY memoryReportingChanged)
    Q_PROPERTY(QVariantMap memoryPss READ memoryPss NOTIFY memoryReportingChanged)
    Q_PROPERTY(bool memoryReportingEnabled READ isMemoryReportingEnabled WRITE setMemoryReportingEnabled
                                           NOTIFY memoryReportingEnabledChanged)
    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT)
public:
    ProcessStatus(QObject *parent = nullptr);
    ~ProcessStatus();

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
    void onRunStateChanged(Am::RunState state);

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
