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

#pragma once

#include <QTest>

// sadly this has to be a define for QVERIFY2() to work
#define AM_CHECK_ERRORSTRING(_actual_errstr, _expected_errstr) do { \
    if (_expected_errstr.startsWith(QLatin1String("~"))) { \
        QRegularExpression re(QStringLiteral("\\A") + _expected_errstr.mid(1) + QStringLiteral("\\z")); \
        QVERIFY2(re.match(_actual_errstr).hasMatch(), \
                 qPrintable("\n    Got     : " + _actual_errstr.toLocal8Bit() + \
                            "\n    Expected: " + _expected_errstr.toLocal8Bit())); \
    } else { \
        QCOMPARE(_actual_errstr, _expected_errstr); \
    } \
} while (false)
