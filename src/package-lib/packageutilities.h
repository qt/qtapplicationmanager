// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

namespace PackageUtilities
{
Q_DECL_DEPRECATED bool ensureCorrectLocale(QStringList *warnings);
bool ensureCorrectLocale();
bool checkCorrectLocale();
}

QT_END_NAMESPACE_AM
