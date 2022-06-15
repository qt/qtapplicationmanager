// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore>
#include <QtTest>

#include "utilities.h"

QT_USE_NAMESPACE_AM

class tst_Utilities : public QObject
{
    Q_OBJECT

public:
    tst_Utilities();

private slots:
};


tst_Utilities::tst_Utilities()
{ }

QTEST_APPLESS_MAIN(tst_Utilities)

#include "tst_utilities.moc"
