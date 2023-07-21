// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)

QT_BEGIN_NAMESPACE_AM

class TestRunner
{
public:
    static void initialize(const QString &testFile, const QStringList &testRunnerArguments,
                           const QString &sourceFile = { });
    static int exec(QQmlEngine *qmlEngine);
};

QT_END_NAMESPACE_AM
