/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include <QMutex>

#include <openssl/evp.h>
#include <openssl/err.h>

#include "cryptography.h"

#ifdef Q_OS_WIN
// all this mess is needed to get RtlGenRandom()
#  include "windows.h"
#  define SystemFunction036 NTAPI SystemFunction036
#  include "ntsecapi.h"
#  undef SystemFunction036
#endif

#ifdef Q_OS_UNIX
#  include <QFile>
#endif

Q_GLOBAL_STATIC(QMutex, initMutex)

QByteArray Cryptography::generateRandomBytes(int size)
{
    QByteArray result;

    if (size > 0) {
#if defined(Q_OS_UNIX)
        QFile f(QLatin1String("/dev/urandom"));
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
    static bool openSslInitialized = false;

    QMutexLocker locker(initMutex());
    if (!openSslInitialized) {
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();
        openSslInitialized = true;
    }
}
