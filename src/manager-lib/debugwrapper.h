// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>

#include <QtCore/QStringList>
#include <QtCore/QMap>

QT_BEGIN_NAMESPACE_AM

namespace DebugWrapper {

bool parseSpecification(const QString &debugWrapperSpecification, QStringList &resultingCommand,
                        QMap<QString, QString> &resultingEnvironment);

QStringList substituteCommand(const QStringList &debugWrapperCommand, const QString &program,
                              const QStringList &arguments);

}

QT_END_NAMESPACE_AM
