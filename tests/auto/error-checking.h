// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef ERROR_CHECKING_H
#define ERROR_CHECKING_H

#include <QTest>

// sadly this has to be a define for QVERIFY2() to work
#define QT_AM_CHECK_ERRORSTRING(_actual_errstr, _expected_errstr) do { \
    if (_expected_errstr.startsWith(u"~")) { \
        QRegularExpression re(_expected_errstr.mid(1)); \
        QVERIFY2(re.match(_actual_errstr).hasMatch(), \
                 QByteArray("\n    Got     : ") + _actual_errstr.toLocal8Bit() + \
                 QByteArray("\n    Expected: ") + _expected_errstr.toLocal8Bit()); \
    } else { \
        QCOMPARE(_actual_errstr, _expected_errstr); \
    } \
} while (false)

#endif // ERROR_CHECKING_H
