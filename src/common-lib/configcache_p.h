// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include "configcache.h"

QT_BEGIN_NAMESPACE_AM

struct ConfigCacheEntry
{
    QString filePath;    // abs. file path
    QByteArray checksum; // sha1 (fast and sufficient for this use-case)
    QByteArray rawContent;  // raw YAML content
    void *content = nullptr;  // parsed YAML content
    bool checksumMatches = false;
};

struct CacheHeader
{
    enum { Magic = 0x23d39366, // dd if=/dev/random bs=4 count=1 status=none | xxd -p
           Version = 3 };

    quint32 magic = Magic;
    quint32 version = Version;
    quint32 typeId = 0;
    quint32 typeVersion = 0;
    QString baseName;
    quint32 entries = 0;

    bool isValid(const QString &baseName, quint32 typeId = 0, quint32 typeVersion = 0) const;
};

class ConfigCachePrivate
{
public:
    AbstractConfigCache::Options options;
    quint32 typeId;
    quint32 typeVersion;
    QStringList rawFiles;
    QString cacheBaseName;
    QVector<ConfigCacheEntry> cache;
    QMap<QString, int> cacheIndex;
    void *mergedContent = nullptr;
    bool cacheWasRead = false;
    bool cacheWasWritten = false;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
