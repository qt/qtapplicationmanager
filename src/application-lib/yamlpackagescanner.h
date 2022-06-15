// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QIODevice>
#include <QtAppManApplication/packagescanner.h>

QT_BEGIN_NAMESPACE_AM


class YamlPackageScanner : public PackageScanner
{
public:
    YamlPackageScanner();

    PackageInfo *scan(const QString &filePath) Q_DECL_NOEXCEPT_EXPR(false) override;
    PackageInfo *scan(QIODevice *source, const QString &filePath) Q_DECL_NOEXCEPT_EXPR(false) override;
};

QT_END_NAMESPACE_AM
