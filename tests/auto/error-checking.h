// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef ERROR_CHECKING_H
#define ERROR_CHECKING_H

#include <QTest>

// sadly this has to be a define for QVERIFY2() to work
#define QT_AM_CHECK_ERRORSTRING(_actual_errstr, _expected_errstr) do { \
    if (_expected_errstr.startsWith(u"~")) { \
        QRegularExpression re(_expected_errstr.mid(1)); \
        QVERIFY2(re.match(_actual_errstr).hasMatch(), \
                 qPrintable(u"\n    Got     : " + _actual_errstr + \
                            u"\n    Expected: " + _expected_errstr)); \
    } else { \
        QCOMPARE(_actual_errstr, _expected_errstr); \
    } \
} while (false)

namespace {
[[maybe_unused]] bool amVerboseTest()
{
    static bool verbose = [] {
        bool v = qEnvironmentVariableIsSet("AM_VERBOSE_TEST");
        qInfo() << "Verbose mode:" << (v ? "on" : "off") << "(change with $AM_VERBOSE_TEST or the -v1/-v2 options)";
        return v;
    }();
    return verbose;
}
}

#define QT_AM_VERBOSE_TEST_MAIN(TestObject, ...) \
    QTEST_MAIN_WRAPPER(TestObject, \
    __VA_ARGS__ \
    for (int i = 1; i < argc; ++i) { \
        if ((qstrlen(argv[i]) == 3) && (argv[i][0] == '-') && (argv[i][1] == 'v') \
            && ((argv[i][2] == '1') || (argv[i][2] == '2'))) { \
            qputenv("AM_VERBOSE_TEST", "1"); break; \
        } \
     })

#endif // ERROR_CHECKING_H
