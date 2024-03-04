// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CONSOLE_H
#define CONSOLE_H

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Console
{
public:
    static bool ensureInitialized();

    static bool stdoutSupportsAnsiColor();
    static bool stderrSupportsAnsiColor();
    static bool isRunningInQtCreator();
    static bool stdoutIsConsoleWindow();
    static bool stderrIsConsoleWindow();
    static int width();
};

QT_END_NAMESPACE_AM

#endif // CONSOLE_H
