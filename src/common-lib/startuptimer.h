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

#include <QObject>
#include <QVector>
#include <QPair>
#include <QByteArray>
#include <QElapsedTimer>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class StartupTimer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint64 timeToFirstFrame READ timeToFirstFrame NOTIFY timeToFirstFrameChanged)
    Q_PROPERTY(quint64 systemUpTime READ systemUpTime NOTIFY systemUpTimeChanged)
    Q_PROPERTY(bool automaticReporting READ automaticReporting WRITE setAutomaticReporting NOTIFY automaticReportingChanged)

public:
    static StartupTimer *instance();
    ~StartupTimer();

    Q_INVOKABLE void checkpoint(const QString &name);
    Q_INVOKABLE void createReport(const QString &title = QString());

    quint64 timeToFirstFrame() const;
    quint64 systemUpTime() const;
    bool automaticReporting() const;

    void checkpoint(const char *name);
    void createAutomaticReport(const QString &title);
    void checkFirstFrame();
    void reset();

public slots:
    void setAutomaticReporting(bool enableAutomaticReporting);

signals:
    void timeToFirstFrameChanged(quint64 timeToFirstFrame);
    void systemUpTimeChanged(quint64 systemUpTime);
    void automaticReportingChanged(bool setAutomaticReporting);

private:
    StartupTimer();
    static StartupTimer *s_instance;

    FILE *m_output = nullptr;
    bool m_initialized = false;
    bool m_automaticReporting = true;
    quint64 m_processCreation = 0;
    quint64 m_timeToFirstFrame = 0;
    quint64 m_systemUpTime = 0;
    QElapsedTimer m_timer;
    QVector<QPair<quint64, QByteArray>> m_checkpoints;

    Q_DISABLE_COPY(StartupTimer)
};

QT_END_NAMESPACE_AM
