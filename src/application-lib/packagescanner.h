// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class PackageInfo;

class PackageScanner
{
public:
    virtual ~PackageScanner() = default;

    virtual PackageInfo *scan(const QString &fileName) noexcept(false) = 0;
    virtual PackageInfo *scan(QIODevice *source, const QString &fileName) noexcept(false) = 0;

protected:
    PackageScanner() = default;

private:
    Q_DISABLE_COPY_MOVE(PackageScanner)
};

QT_END_NAMESPACE_AM
