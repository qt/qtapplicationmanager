// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

namespace Cryptography {

QByteArray generateRandomBytes(int size);

void initialize();

void enableOpenSsl3LegacyProvider(); // needs to be called before any other crypto functions

QString errorString(qint64 osCryptoError, const char *errorDescription = nullptr);

}

QT_END_NAMESPACE_AM
