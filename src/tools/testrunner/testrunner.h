// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef TESTRUNNER_H
#define TESTRUNNER_H

#include <QStringList>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)

QT_BEGIN_NAMESPACE_AM

class Configuration;

class TestRunner
{
public:
    static void setup(Configuration *cfg);
    static int exec(QQmlEngine *qmlEngine);
};

QT_END_NAMESPACE_AM

#endif // TESTRUNNER_H
