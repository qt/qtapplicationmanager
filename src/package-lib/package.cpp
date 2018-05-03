/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QByteArray>
#include <QString>

#include "package.h"

QT_BEGIN_NAMESPACE_AM

bool Package::ensureCorrectLocale()
{
    // We need to make sure we are running in a Unicode locale, since we are
    // running into problems when unpacking packages with libarchive that
    // contain non-ASCII characters.
    // This has to be done *before* the QApplication constructor to avoid
    // the initialization of the text codecs.

#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    // Windows is UTF16
    // Mac is UTF8
    return true;
#else

    // check if umlaut-a converts correctly
    auto checkUtf = []() -> bool {
        const wchar_t umlaut_w[2] = { 0x00e4, 0x0000 };
        char umlaut_mb[2];
        size_t umlaut_mb_len = wcstombs(umlaut_mb, umlaut_w, sizeof(umlaut_mb));

        return (umlaut_mb_len == 2 && umlaut_mb[0] == '\xc3' && umlaut_mb[1] == '\xa4');
    };

    // init locales from env variables
    setlocale(LC_ALL, "");

    // check if this is enough to have an UTF-8 locale
    if (checkUtf())
        return true;

    // LC_ALL trumps all the rest, so if this is not specifying an UTF-8 locale, we need to unset it
    QByteArray lc_all = qgetenv("LC_ALL");
    if (!lc_all.isEmpty() && !(lc_all.endsWith(".UTF-8") || lc_all.endsWith(".UTF_8"))) {
        unsetenv("LC_ALL");
        setlocale(LC_ALL, "");
    }

    // check again
    if (checkUtf())
        return true;

    // now the time-consuming part: trying to switch to a well-known UTF-8 locale
    const char *locales[] = { "C.UTF-8", "en_US.UTF-8", nullptr };
    for (const char **loc = locales; *loc; ++loc) {
        if (const char *old = setlocale(LC_CTYPE, *loc)) {
            if (checkUtf()) {
                if (setenv("LC_CTYPE", *loc, 1) == 0)
                    return true;
            }
            setlocale(LC_CTYPE, old);
        }
    }

    return false;
#endif
}


bool Package::checkCorrectLocale()
{
    // see ensureCorrectLocale() above. Call this after the QApplication
    // constructor as a sanity check.
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    return true;
#else
    // check if umlaut-a converts correctly
    return QString::fromUtf8("\xc3\xa4").toLocal8Bit() == "\xc3\xa4";
#endif
}

QT_END_NAMESPACE_AM
