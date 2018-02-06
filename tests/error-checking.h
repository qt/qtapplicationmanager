/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
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
