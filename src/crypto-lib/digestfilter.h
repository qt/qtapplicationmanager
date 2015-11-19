/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
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

#pragma once

#include <QString>
#include <QByteArray>

class DigestFilterPrivate;

class DigestFilter
{
public:
    enum Type { Sha1, Sha256, Sha512 };

    static QString nameFromType(Type t);
    static Type typeFromName(const QString &name, bool *ok = 0);

    explicit DigestFilter(Type t);
    ~DigestFilter();

    Type type() const;

    int size() const;

    bool start();
    bool processData(const char *data, int size);
    bool processData(const QByteArray &data);
    bool finish(QByteArray &digest);

    QString errorString() const;

    static QByteArray digest(Type t, const QByteArray &data);

protected:
    DigestFilter(Type t, const QByteArray &hmacKey);

private:
    DigestFilterPrivate *d;
};

class HMACFilter : public DigestFilter
{
public:
    explicit HMACFilter(Type t, const QByteArray &key);

    static QByteArray hmac(Type t, const QByteArray &key, const QByteArray &data);

private:
    using DigestFilter::digest;
};
