// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <mutex>

#include "global.h"
#include "cryptography.h"

#if defined(Q_OS_UNIX)
#  include <QFile>
#elif defined(Q_OS_WIN)
// all this mess is needed to get RtlGenRandom()
#  include "windows.h"
#  define SystemFunction036 NTAPI SystemFunction036
#  include "ntsecapi.h"
#  undef SystemFunction036
#endif
#if defined(Q_OS_MACOS)
#  include <QVersionNumber>
#endif
#if defined(QT_AM_USE_LIBCRYPTO)
#  include "libcryptofunction.h"
#endif


using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

QByteArray Cryptography::generateRandomBytes(int size)
{
    QByteArray result;

    if (size > 0) {
#if defined(Q_OS_UNIX)
        QFile f(u"/dev/urandom"_s);
        if (f.open(QIODevice::ReadOnly)) {
            result = f.read(size);
            if (result.size() != size)
                result.clear();
        }
#elif defined(Q_OS_WIN)
        result.resize(size);
        if (!RtlGenRandom(result.data(), size))
            result.clear();
#endif
    }
    return result;
}

void Cryptography::initialize()
{
#if defined(QT_AM_USE_LIBCRYPTO)
    static std::once_flag once;
    std::call_once(once, []() { LibCryptoFunctionBase::initialize(); });
#endif
}

QT_END_NAMESPACE_AM
