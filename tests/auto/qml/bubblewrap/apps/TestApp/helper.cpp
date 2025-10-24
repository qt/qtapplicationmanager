// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/QtEnvironmentVariables>
#include "helper.h"

Helper::Helper(QObject *parent)
    : QObject{parent}
{}

QByteArray Helper::getEnv(const QByteArray &envName)
{
    return qgetenv(envName.constData());
}
