// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDataStream>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QBuffer>
#include <QtConcurrent/QtConcurrent>

#include "configcache.h"
#include "configcache_p.h"
#include "utilities.h"
#include "exception.h"
#include "logging.h"

// use QtConcurrent to parse the files, if there are more than x files
#define AM_PARALLEL_THRESHOLD  1


QT_BEGIN_NAMESPACE_AM

QDataStream &operator>>(QDataStream &ds, ConfigCacheEntry &ce)
{
    bool contentValid = false;
    ds >> ce.filePath >> ce.checksum >> contentValid;
    ce.rawContent.clear();
    ce.content = contentValid ? reinterpret_cast<void *>(-1) : nullptr;
    return ds;
}

QDataStream &operator<<(QDataStream &ds, const ConfigCacheEntry &ce)
{
    ds << ce.filePath << ce.checksum << static_cast<bool>(ce.content);
    return ds;
}

QDataStream &operator>>(QDataStream &ds, CacheHeader &ch)
{
    ds >> ch.magic >> ch.version >> ch.typeId >> ch.typeVersion >> ch.baseName >> ch.entries;
    return ds;
}

QDataStream &operator<<(QDataStream &ds, const CacheHeader &ch)
{
    ds << ch.magic << ch.version << ch.typeId << ch.typeVersion << ch.baseName << ch.entries;
    return ds;
}

QDebug operator<<(QDebug dbg, const ConfigCacheEntry &ce)
{
    dbg << "CacheEntry {\n  " << ce.filePath << "\n  " << ce.checksum.toHex() << "\n  valid:"
        << (ce.content ? "yes" : "no") << ce.content
        << "\n}\n";
    return dbg;
}


static quint32 makeTypeId(const char typeIdStr[4])
{
    if (typeIdStr) {
        return (quint32(typeIdStr[0])) | (quint32(typeIdStr[1]) << 8)
                | (quint32(typeIdStr[2]) << 16) | (quint32(typeIdStr[3]) << 24);
    } else {
        return 0;
    }
}

bool CacheHeader::isValid(const QString &baseName, quint32 typeId, quint32 typeVersion) const
{
    return magic == Magic
            && version == Version
            && this->typeId == typeId
            && this->typeVersion == typeVersion
            && this->baseName == baseName
            && entries < 1000;
}


AbstractConfigCache::AbstractConfigCache(const QStringList &configFiles, const QString &cacheBaseName,
                                         const char typeId[4], quint32 version, Options options)
    : d(new ConfigCachePrivate)
{
    d->options = options;
    d->typeId = makeTypeId(typeId);
    d->typeVersion = version;
    d->rawFiles = configFiles;
    d->cacheBaseName = cacheBaseName;
}

AbstractConfigCache::~AbstractConfigCache()
{
    // make sure that clear() was called in ~Cache(), since we need the virtual destruct() function!
    delete d;
}

void *AbstractConfigCache::takeMergedResult() const
{
    Q_ASSERT(d->options & MergedResult);
    void *result = d->mergedContent;
    d->mergedContent = nullptr;
    return result;
}

void *AbstractConfigCache::takeResult(int index) const
{
    Q_ASSERT(!(d->options & MergedResult));
    void *result = nullptr;
    if (index >= 0 && index < d->cache.size())
        std::swap(result, d->cache[index].content);
    return result;
}

void *AbstractConfigCache::takeResult(const QString &rawFile) const
{
    return takeResult(d->cacheIndex.value(rawFile, -1));
}

void AbstractConfigCache::parse()
{
    clear();

    if (d->rawFiles.isEmpty())
        return;

    QElapsedTimer timer;
    if (LogCache().isDebugEnabled())
        timer.start();

    // normalize all yaml file names
    QStringList rawFilePaths;
    for (const auto &rawFile : qAsConst(d->rawFiles)) {
        const auto path = QFileInfo(rawFile).canonicalFilePath();
        if (path.isEmpty())
            throw Exception("file %1 does not exist").arg(rawFile);
        if (rawFilePaths.contains(path))
            throw Exception("duplicate files are not allowed - found %1 at least two times").arg(path);
        rawFilePaths << path;
    }

    QFile cacheFile(cacheFilePath());

    QAtomicInt cacheIsValid = false;
    QAtomicInt cacheIsComplete = false;

    QVector<ConfigCacheEntry> cache;
    void *mergedContent = nullptr;

    qCDebug(LogCache) << d->cacheBaseName << "cache file:" << cacheFile.fileName();
    qCDebug(LogCache) << d->cacheBaseName << "read cache?" << ((d->options & (ClearCache | NoCache)) ? "no" : "yes")
                      << "/ write cache?" << ((d->options & NoCache) ? "no" : "yes");
    qCDebug(LogCache) << d->cacheBaseName << "reading:" << rawFilePaths;

    if (!d->options.testFlag(NoCache) && !d->options.testFlag(ClearCache)) {
        if (cacheFile.open(QFile::ReadOnly)) {
            try {
                QDataStream ds(&cacheFile);
                CacheHeader cacheHeader;
                ds >> cacheHeader;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("failed to read cache header");
                if (!cacheHeader.isValid(d->cacheBaseName, d->typeId, d->typeVersion))
                    throw Exception("failed to parse cache header");

                cache.resize(int(cacheHeader.entries));
                for (int i = 0; i < int(cacheHeader.entries); ++i) {
                    ConfigCacheEntry &ce = cache[i];
                    ds >> ce;
                    if (ce.content)
                        ce.content = loadFromCache(ds);
                }
                if (d->options & MergedResult) {
                    bool hasMerged = false;
                    ds >> hasMerged;
                    if (hasMerged)
                        mergedContent = loadFromCache(ds);

                    if (!mergedContent)
                        throw Exception("failed to read merged cache content");
                }

                if (ds.status() != QDataStream::Ok)
                    throw Exception("failed to read cache content (%1)").arg(ds.status());

                cacheIsValid = true;

                qCDebug(LogCache) << d->cacheBaseName << "loaded" << cache.size() << "entries in"
                                  << timer.nsecsElapsed() / 1000 << "usec";

                // check if we can use the cache as-is, or if we need to cherry-pick parts
                if (rawFilePaths.count() == cache.count()) {
                    cacheIsComplete = true;

                    for (int i = 0; i < rawFilePaths.count(); ++i) {
                        const ConfigCacheEntry &ce = cache.at(i);

                        if ((rawFilePaths.at(i) != ce.filePath) || !ce.content)
                            cacheIsComplete = false;
                    }
                }
                d->cacheWasRead = true;

            } catch (const Exception &e) {
                qWarning(LogCache) << "Failed to read cache:" << e.what();
            }
            cacheFile.close();
        }
    } else if (d->options.testFlag(ClearCache)) {
        cacheFile.remove();
    }

    qCDebug(LogCache) << d->cacheBaseName << "valid:" << (cacheIsValid ? "yes" : "no")
                      << "/ complete:" << (cacheIsComplete ? "yes" : "no");

    if (!cacheIsComplete) {
        // we need to pick the parts we can re-use

        QVector<ConfigCacheEntry> newCache(rawFilePaths.size());

        // we are iterating over n^2 entries in the worst case scenario -- we could reduce it to n
        // by using a QHash or QMap, but that doesn't come for free either: especially given the
        // low number of processed entries (well under 100 for app manifests; around a couple for
        // config files)
        for (int i = 0; i < rawFilePaths.size(); ++i) {
            const QString &rawFilePath = rawFilePaths.at(i);
            ConfigCacheEntry &ce = newCache[i];

            // if we already got this file in the cache, then use the entry
            bool found = false;
            for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
                if ((it->filePath == rawFilePath) && it->content) {
                    ce = *it;
                    found = true;
                    qCDebug(LogCache) << d->cacheBaseName << "found cache entry for" << it->filePath;
                    break;
                }
            }

            // if it's not yet cached, then add it to the list
            if (!found) {
                ce.filePath = rawFilePath;
                qCDebug(LogCache) << d->cacheBaseName << "missing cache entry for" << rawFilePath;
            }
        }
        cache = newCache;
    }

    // reads a single config file and calculates its hash - defined as lambda to be usable
    // both via QtConcurrent and via std:for_each
    auto readConfigFile = [&cacheIsComplete, this](ConfigCacheEntry &ce) {
        QFile file(ce.filePath);
        if (!file.open(QIODevice::ReadOnly))
            throw Exception("Failed to open file '%1' for reading.\n").arg(file.fileName());

        if (file.size() > 1024*1024)
            throw Exception("File '%1' is too big (> 1MB).\n").arg(file.fileName());

        ce.rawContent = file.readAll();
        preProcessSourceContent(ce.rawContent, ce.filePath);

        QByteArray checksum = QCryptographicHash::hash(ce.rawContent, QCryptographicHash::Sha1);
        ce.checksumMatches = (checksum == ce.checksum);
        ce.checksum = checksum;
        if (!ce.checksumMatches) {
            if (ce.content) {
                qWarning(LogCache) << "Failed to read Cache: cached file checksums do not match";
                destruct(ce.content);
                ce.content = nullptr;
            }
            cacheIsComplete = false;
        }
    };

    // these can throw
    if (cache.size() > AM_PARALLEL_THRESHOLD)
        QtConcurrent::blockingMap(cache, readConfigFile);
    else
        std::for_each(cache.begin(), cache.end(), readConfigFile);

    qCDebug(LogCache) << d->cacheBaseName << "reading all of" << cache.size() << "file(s) finished after"
                      << (timer.nsecsElapsed() / 1000) << "usec";
    qCDebug(LogCache) << d->cacheBaseName << "still complete:" << (cacheIsComplete ? "yes" : "no");

    if (!cacheIsComplete) {
        // we have read a partial cache or none at all - parse what's not cached yet
        if (d->options & MergedResult) {
            destruct(mergedContent);
            mergedContent = nullptr;
        }

        QAtomicInt count;

        auto parseConfigFile = [this, &count](ConfigCacheEntry &ce) {
            if (ce.content)
                return;

            ++count;
            try {
                QBuffer buffer(&ce.rawContent);
                buffer.open(QIODevice::ReadOnly);
                ce.content = loadFromSource(&buffer, ce.filePath);
            } catch (const Exception &e) {
                if (d->options.testFlag(IgnoreBroken)) {
                    ce.content = nullptr;
                } else {
                    throw Exception("Could not parse file '%1': %2")
                            .arg(ce.filePath).arg(e.errorString());
                }
            }
        };

        // these can throw
        if (cache.size() > AM_PARALLEL_THRESHOLD)
            QtConcurrent::blockingMap(cache, parseConfigFile);
        else
            std::for_each(cache.begin(), cache.end(), parseConfigFile);

        if (d->options & MergedResult) {
            // we cannot parallelize this step, since subsequent config files can overwrite
            // or append to values
            for (int i = 0; i < cache.size(); ++i) {
                ConfigCacheEntry &ce = cache[i];
                if (ce.content) {
                    if (!mergedContent)
                        mergedContent = clone(ce.content);
                    else
                        merge(mergedContent, ce.content);
                }
            }
        }

        qCDebug(LogCache) << d->cacheBaseName << "parsing" << count.loadAcquire()
                          << "file(s) finished after" << (timer.nsecsElapsed() / 1000) << "usec";

        if (!d->options.testFlag(NoCache)) {
            // everything is parsed now, so we can write a new cache file

            try {
                QFile newCacheFile(cacheFile.fileName());
                if (!newCacheFile.open(QFile::WriteOnly | QFile::Truncate))
                    throw Exception(cacheFile, "failed to open file for writing");

                QDataStream ds(&newCacheFile);
                CacheHeader cacheHeader;
                cacheHeader.baseName = d->cacheBaseName;
                cacheHeader.typeId = d->typeId;
                cacheHeader.typeVersion = d->typeVersion;
                cacheHeader.entries = quint32(cache.size());
                ds << cacheHeader;

                for (int i = 0; i < cache.size(); ++i) {
                    const ConfigCacheEntry &ce = cache.at(i);
                    ds << ce;
                    // qCDebug(LogCache) << "SAVING" << ce << ce.content;
                    if (ce.content)
                        saveToCache(ds, ce.content);
                }

                if (d->options & MergedResult) {
                    ds << bool(mergedContent);
                    if (mergedContent)
                        saveToCache(ds, mergedContent);
                }

                if (ds.status() != QDataStream::Ok)
                    throw Exception("error writing content");

                d->cacheWasWritten = true;
            } catch (const Exception &e) {
                qCWarning(LogCache) << "Failed to write Cache:" << e.what();
            }
            qCDebug(LogCache) << d->cacheBaseName << "writing the cache finished after"
                              << (timer.nsecsElapsed() / 1000) << "usec";
        }
    }

    d->cache = cache;
    if (d->options & MergedResult)
        d->mergedContent = mergedContent;

    qCDebug(LogCache) << d->cacheBaseName << "finished cache parsing after"
                      << (timer.nsecsElapsed() / 1000) << "usec";
}

void AbstractConfigCache::clear()
{
    for (auto &ce : qAsConst(d->cache))
        destruct(ce.content);
    d->cache.clear();
    d->cacheIndex.clear();
    destruct(d->mergedContent);
    d->mergedContent = nullptr;
    d->cacheWasRead = false;
    d->cacheWasWritten = false;
}

bool AbstractConfigCache::parseReadFromCache() const
{
    return d->cacheWasRead;
}

bool AbstractConfigCache::parseWroteToCache() const
{
    return d->cacheWasWritten;
}

QString AbstractConfigCache::cacheFilePath() const
{
    // find the correct cache location and make sure it exists
    static const QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.exists())
        dir.mkpath(qSL("."));
    return dir.absoluteFilePath(qSL("appman-%1.cache").arg(d->cacheBaseName));
}

QT_END_NAMESPACE_AM
