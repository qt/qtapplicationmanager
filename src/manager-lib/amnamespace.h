/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

// a namespace with Q_NAMESPACE would be better suited, but this leads to weird issues with the
// enums when used as Q_PROPERTY types and accessed from QML
class Am : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/Am 2.0 UNCREATABLE")

public:
    // we cannot use QProcess enums directly, since some supported platforms might
    // not have QProcess available at all.
    // keep these enums in sync with those in containerinterface.h
    enum ExitStatus {
        NormalExit,
        CrashExit,
        ForcedExit
    };
    Q_ENUM(ExitStatus)

    enum RunState {
        NotRunning,
        StartingUp,
        Running,
        ShuttingDown,
    };
    Q_ENUM(RunState)

    enum ProcessError {
        FailedToStart, //### file not found, resource error
        Crashed,
        Timedout,
        ReadError,
        WriteError,
        UnknownError
    };
    Q_ENUM(ProcessError)
};

QT_END_NAMESPACE_AM
