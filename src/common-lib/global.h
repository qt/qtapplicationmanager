// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once
#include <QtCore/qglobal.h>
#include <QtAppManCommon/qtappman_common-config.h>

#define QT_BEGIN_NAMESPACE_AM  namespace QtAM {
#define QT_END_NAMESPACE_AM    }
#define QT_USE_NAMESPACE_AM    using namespace QtAM;
#define QT_PREPEND_NAMESPACE_AM(name) QtAM::name

QT_BEGIN_NAMESPACE_AM
// make sure the namespace exists
QT_END_NAMESPACE_AM

// make the source a lot less ugly and more readable (until we can finally use user defined literals)
#define qL1S(x) QLatin1String(x)
#define qL1C(x) QLatin1Char(x)
#define qSL(x) QStringLiteral(x)

