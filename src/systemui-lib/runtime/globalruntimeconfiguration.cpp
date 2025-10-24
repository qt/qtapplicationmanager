// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "globalruntimeconfiguration.h"

QT_BEGIN_NAMESPACE_AM

GlobalRuntimeConfiguration &GlobalRuntimeConfiguration::instance()
{
    static GlobalRuntimeConfiguration inst;
    return inst;
}

QT_END_NAMESPACE_AM
