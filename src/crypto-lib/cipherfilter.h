/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
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

#pragma once

#include <QString>
#include <QByteArray>

class CipherFilterPrivate;

class CipherFilter
{
public:
    enum Cipher
    {
        None = 0,
        AES_128_CFB,
        AES_128_OFB,
        AES_128_CBC,

        AES_256_CFB,
        AES_256_OFB,
        AES_256_CBC,

        Default = AES_128_CFB
    };

    enum Type { Encrypt, Decrypt };

    explicit CipherFilter(Type t, Cipher cipher = Default);
    ~CipherFilter();

    Type type() const;

    int keySize() const;
    int ivSize() const;
    int blockSize() const;

    bool start(const char *key, int keyLen, const char *iv, int ivLen);
    bool processData(const QByteArray &src, QByteArray &dst);
    bool finish(QByteArray &dst);

    QString errorString() const;

private:
    bool internalProcessData(const QByteArray &src, QByteArray &dst, bool lastChunk);
    CipherFilterPrivate *d;
};
