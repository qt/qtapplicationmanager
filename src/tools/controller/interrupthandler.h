// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <functional>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class InterruptHandler
{
public:
    static void install(const std::function<void(int)> &handler);
};

QT_END_NAMESPACE_AM
