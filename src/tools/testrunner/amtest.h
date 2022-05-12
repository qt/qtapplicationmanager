/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2020 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

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
    Q_INVOKABLE bool dirExists(const QString &dir);
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
