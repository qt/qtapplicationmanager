/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

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

#if defined(Q_OS_OSX)
#  include <QtCore/private/qcore_mac_p.h>
#  include <Security/SecBase.h>
#endif

#if defined(AM_USE_LIBCRYPTO)
#  include "libcryptofunction.h"
#  include <openssl/err.h>

Q_GLOBAL_STATIC(QMutex, initMutex)

static AM_LIBCRYPTO_FUNCTION(ERR_error_string_n);

#endif


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
#if defined(AM_USE_LIBCRYPTO)
    static bool openSslInitialized = false;

    QMutexLocker locker(initMutex());
    if (!openSslInitialized) {
        if (!LibCryptoFunctionBase::initialize())
            qFatal("Could not load libcrypto");
        openSslInitialized = true;
    }
#endif
}

QString Cryptography::errorString(qint64 osCryptoError, const char *errorDescription)
{
    QString result;
    if (errorDescription && *errorDescription) {
        result = QString::fromLatin1(errorDescription);
        result.append(QLatin1String(": "));
    }

#if defined(AM_USE_LIBCRYPTO)
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
        result.append(QString::fromWCharArray(msg));
        HeapFree(GetProcessHeap(), 0, msg);
    }
#elif defined(Q_OS_OSX)
    if (osCryptoError) {
        QCFType<CFStringRef> msg = SecCopyErrorMessageString(osCryptoError, nullptr);
        result.append(QString::fromCFString(msg));
    }
#endif

    return result;
}
