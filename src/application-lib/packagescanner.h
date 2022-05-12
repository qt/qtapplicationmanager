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

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class PackageInfo;

class PackageScanner
{
public:
    virtual ~PackageScanner() = default;

    virtual PackageInfo *scan(const QString &fileName) Q_DECL_NOEXCEPT_EXPR(false) = 0;
    virtual PackageInfo *scan(QIODevice *source, const QString &fileName) Q_DECL_NOEXCEPT_EXPR(false) = 0;

protected:
    PackageScanner() = default;

private:
    Q_DISABLE_COPY(PackageScanner)
};

QT_END_NAMESPACE_AM
