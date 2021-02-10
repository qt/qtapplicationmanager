/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

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
