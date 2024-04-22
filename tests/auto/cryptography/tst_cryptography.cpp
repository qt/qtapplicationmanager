// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "cryptography.h"

QT_USE_NAMESPACE_AM

class tst_Cryptography : public QObject
{
    Q_OBJECT

public:
    tst_Cryptography();

private Q_SLOTS:
    void random();
};

tst_Cryptography::tst_Cryptography()
{ }

void tst_Cryptography::random()
{
    QVERIFY(Cryptography::generateRandomBytes(-1).isEmpty());
    QVERIFY(Cryptography::generateRandomBytes(0).isEmpty());
    QVERIFY(!Cryptography::generateRandomBytes(1).isEmpty());
    QCOMPARE(Cryptography::generateRandomBytes(128).size(), 128);
}

QTEST_APPLESS_MAIN(tst_Cryptography)

#include "tst_cryptography.moc"
