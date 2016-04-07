/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QVector>
#include <QPair>
#include <QByteArray>
#include <QElapsedTimer>

class StartupTimer
{
public:
    // this should be the first instruction in main()
    StartupTimer();
    ~StartupTimer();

    void checkpoint(const char *name);
    void createReport() const;

private:
    FILE *m_output = 0;
    bool m_initialized = false;
    quint64 m_processCreation = 0;
    QElapsedTimer m_timer;
    QVector<QPair<quint64, QByteArray>> m_checkpoints;

    Q_DISABLE_COPY(StartupTimer)
};
