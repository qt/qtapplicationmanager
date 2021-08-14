/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2020 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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
****************************************************************************/

#pragma once

#include <QObject>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AmTest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int timeoutFactor READ timeoutFactor CONSTANT)

    AmTest();

public:
    enum MsgType { DebugMsg, WarningMsg, CriticalMsg, FatalMsg, InfoMsg, SystemMsg = CriticalMsg };
    Q_ENUM(MsgType)

    static AmTest *instance();

    int timeoutFactor() const;

    Q_INVOKABLE void ignoreMessage(QT_PREPEND_NAMESPACE_AM(AmTest::MsgType) type, const char* msg);
    Q_INVOKABLE void ignoreMessage(QT_PREPEND_NAMESPACE_AM(AmTest::MsgType) type, const QRegularExpression &expression);
    Q_INVOKABLE int observeObjectDestroyed(QObject *obj);
    Q_INVOKABLE void aboutToBlock();
#if defined(Q_OS_LINUX)
    Q_INVOKABLE QString ps(int pid);
    Q_INVOKABLE QString cmdLine(int pid);
    Q_INVOKABLE QString environment(int pid);
    Q_INVOKABLE int findChildProcess(int ppid, const QString &substr);
#endif

Q_SIGNALS:
    void objectDestroyed(int index);
};

QT_END_NAMESPACE_AM
