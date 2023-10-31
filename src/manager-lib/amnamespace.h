// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

// a namespace with Q_NAMESPACE would be better suited, but this leads to weird issues with the
// enums when used as Q_PROPERTY types and accessed from QML
class Am : public QObject
{
    Q_OBJECT

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
