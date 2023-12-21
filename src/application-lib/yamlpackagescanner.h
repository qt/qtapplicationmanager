// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QIODevice>
#include <QtAppManApplication/packagescanner.h>

QT_BEGIN_NAMESPACE_AM


class YamlPackageScanner : public PackageScanner
{
public:
    YamlPackageScanner() = default;

    PackageInfo *scan(const QString &filePath) noexcept(false) override;
    PackageInfo *scan(QIODevice *source, const QString &filePath) noexcept(false) override;
};

QT_END_NAMESPACE_AM
