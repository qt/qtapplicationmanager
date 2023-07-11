// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <memory>
#include <QMap>
#include <QLockFile>
#include "psconfiguration.h"


class PSPackages;
class PSPackage;


class PSPackagesPrivate
{
public:
    void scanPackages();
    void scanUploads();
    void scanRemoves();

    PSPackages *q = nullptr;
    PSConfiguration *cfg = nullptr;
    QMap<QString, QMap<QString, PSPackage *>> packages; // by-id, by-architecture
    std::unique_ptr<QLockFile> lockFile;
    QByteArray lockFilePath; // for the signal handler
};
