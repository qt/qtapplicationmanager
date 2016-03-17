/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QByteArray>
#include <QPair>
#include <QObject>

class CpuReader
{
public:
    bool open() { return false; }
    QPair<int, qreal> readLoadValue() { return qMakePair(1, 0); }
};

class MemoryReader
{
public:
    bool open() { return false; }
    quint64 totalValue() const { return 0; }
    quint64 readUsedValue() const { return 0; }
};

class IoReader
{
public:
    IoReader(const char *) { }
    bool open() { return false; }
    QPair<int, qreal> readLoadValue() { return qMakePair(1, 0); }
};

class MemoryThreshold : public QObject
{
    Q_OBJECT

public:
    MemoryThreshold(const QList<qreal> &thresholds)
        : m_thresholds(thresholds)
    { }
    QList<qreal> thresholdPercentages() const { return m_thresholds; }

    bool isEnabled() const { return false; }
    bool setEnabled(bool on) { return !on; }

signals:
    void thresholdTriggered();

private:
    QList<qreal> m_thresholds;
};
