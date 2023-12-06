// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QMutex>

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
#  include <QtCore/private/qcore_mac_p.h>
#  include <Security/SecBase.h>
#  include <Availability.h>
#endif

#if defined(QT_AM_USE_LIBCRYPTO)
#  include "libcryptofunction.h"

QT_BEGIN_NAMESPACE_AM

Q_GLOBAL_STATIC(QMutex, initMutex)
static bool openSslInitialized = false;
static bool loadOpenSsl3LegacyProvider = false;

// clazy:excludeall=non-pod-global-static
static QT_AM_LIBCRYPTO_FUNCTION(ERR_error_string_n, void(*)(unsigned long, char *, size_t));

QT_END_NAMESPACE_AM

#endif

QT_BEGIN_NAMESPACE_AM


QByteArray Cryptography::generateRandomBytes(int size)
{
    QByteArray result;

    if (size > 0) {
#if defined(Q_OS_UNIX)
        QFile f(qSL("/dev/urandom"));
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
    QMutexLocker locker(initMutex());
    if (!openSslInitialized) {
        if (!LibCryptoFunctionBase::initialize(loadOpenSsl3LegacyProvider))
            qFatal("Could not load libcrypto");
        openSslInitialized = true;
    }
#endif
}

void Cryptography::enableOpenSsl3LegacyProvider()
{
#if defined(QT_AM_USE_LIBCRYPTO)
    QMutexLocker locker(initMutex());
    if (openSslInitialized)
        qCritical("Cryptography::enableOpenSsl3LegacyProvider() needs to be called before using any other crypto functions.");
    else
        loadOpenSsl3LegacyProvider = true;
#endif
}

QString Cryptography::errorString(qint64 osCryptoError, const char *errorDescription)
{
    QString result;
    if (errorDescription && *errorDescription) {
        result = QString::fromLatin1(errorDescription);
        result.append(QLatin1String(": "));
    }

#if defined(QT_AM_USE_LIBCRYPTO)
    if (osCryptoError) {
        char msg[512];
        msg[sizeof(msg) - 1] = 0;

        //void ERR_error_string_n(unsigned long e, char *buf, size_t len);
        am_ERR_error_string_n((unsigned long) osCryptoError, msg, sizeof(msg) - 1);
        result.append(QString::fromLocal8Bit(msg));
    }
#elif defined(Q_OS_WIN)
    if (osCryptoError) {
        LPWSTR msg = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, osCryptoError, 0, (LPWSTR) &msg, 0, nullptr);
        // remove potential \r\n at the end
        result.append(QString::fromWCharArray(msg).trimmed());
        HeapFree(GetProcessHeap(), 0, msg);
    }
#elif defined(Q_OS_MACOS)
    if (osCryptoError) {
#  if QT_MACOS_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(100300, 110300)
        if (__builtin_available(macOS 10.3, iOS 12.3, *)) {
            QCFType<CFStringRef> msg = SecCopyErrorMessageString(qint32(osCryptoError), nullptr);
            result.append(QString::fromCFString(msg));
        }
#  else
        result.append(QString::number(osCryptoError));
#  endif
    }
#else
    Q_UNUSED(osCryptoError)
#endif

    return result;
}

QT_END_NAMESPACE_AM
