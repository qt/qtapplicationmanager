/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
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

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <QtCore/private/qcore_mac_p.h>
#  include <Security/SecBase.h>
#  include <Availability.h>
#endif

#if defined(AM_USE_LIBCRYPTO)
#  include "libcryptofunction.h"

QT_BEGIN_NAMESPACE_AM

Q_GLOBAL_STATIC(QMutex, initMutex)

// clazy:excludeall=non-pod-global-static
static AM_LIBCRYPTO_FUNCTION(ERR_error_string_n, void(*)(unsigned long, char *, size_t));

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
        // remove potential \r\n at the end
        result.append(QString::fromWCharArray(msg).trimmed());
        HeapFree(GetProcessHeap(), 0, msg);
    }
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
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
#endif

    return result;
}

QT_END_NAMESPACE_AM
