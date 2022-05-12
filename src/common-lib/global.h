/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

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

