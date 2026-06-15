// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

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
#include "exception.h"
#include "logging.h"
#include "sudo/sudo.h"

using namespace Qt::StringLiterals;

// use QtConcurrent to parse the files, if there are more than x files
constexpr int AM_PARALLEL_THRESHOLD = 1;


QT_BEGIN_NAMESPACE_AM

QDataStream &operator>>(QDataStream &ds, ConfigCacheEntry &ce)
{
    bool contentValid = false;
    ds >> ce.m_filePath >> ce.m_checksum >> contentValid;
    ce.m_rawContent.clear();
    ce.m_content = contentValid ? reinterpret_cast<void *>(-1) : nullptr;
    return ds;
}

QDataStream &operator<<(QDataStream &ds, const ConfigCacheEntry &ce)
{
    ds << ce.m_filePath << ce.m_checksum << static_cast<bool>(ce.m_content);
    return ds;
}

QDataStream &operator>>(QDataStream &ds, CacheHeader &ch)
{
    ds >> ch.m_magic >> ch.m_version >> ch.m_typeId >> ch.m_typeVersion >> ch.m_baseName >> ch.m_entries;
    return ds;
}

QDataStream &operator<<(QDataStream &ds, const CacheHeader &ch)
{
    ds << ch.m_magic << ch.m_version << ch.m_typeId << ch.m_typeVersion << ch.m_baseName << ch.m_entries;
    return ds;
}

QDebug operator<<(QDebug dbg, const ConfigCacheEntry &ce)
{
    dbg << "CacheEntry {\n  " << ce.m_filePath << "\n  " << ce.m_checksum.toHex() << "\n  valid:"
        << (ce.m_content ? "yes" : "no") << ce.m_content
        << "\n}\n";
    return dbg;
}


static quint32 makeTypeId(std::array<char, 4> typeIdStr)
{
    return (quint32(typeIdStr[0])) | (quint32(typeIdStr[1]) << 8)
           | (quint32(typeIdStr[2]) << 16) | (quint32(typeIdStr[3]) << 24);
}

bool CacheHeader::isValid(const QString &baseName, quint32 typeId, quint32 typeVersion) const
{
    return m_magic == Magic
           && m_version == Version
           && m_typeId == typeId
           && m_typeVersion == typeVersion
           && m_baseName == baseName
           && m_entries < 1000;
}


AbstractConfigCache::AbstractConfigCache(const QStringList &configFiles, const QString &cacheBaseName,
                                         std::array<char, 4> typeId, quint32 version, Options options)
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

void AbstractConfigCache::setExpectedSourceDigests(const QHash<QString, QByteArray> &expectedByPath)
{
    d->expectedSourceDigests = expectedByPath;
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
        std::swap(result, d->cache[index].m_content);
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
    rawFilePaths.reserve(d->rawFiles.size());
    QHash<QString, QByteArray> expectedByCanonical;
    expectedByCanonical.reserve(d->expectedSourceDigests.size());
    for (const auto &rawFile : std::as_const(d->rawFiles)) {
        const auto path = QFileInfo(rawFile).canonicalFilePath();
        if (path.isEmpty())
            throw Exception("file %1 does not exist").arg(rawFile);
        if (rawFilePaths.contains(path))
            throw Exception("duplicate files are not allowed - found %1 at least two times").arg(path);
        rawFilePaths << path;
        if (auto it = d->expectedSourceDigests.constFind(rawFile); it != d->expectedSourceDigests.cend())
            expectedByCanonical.insert(path, *it);
    }

    QAtomicInt cacheIsValid = false;
    QAtomicInt cacheIsComplete = false;

    QVector<ConfigCacheEntry> cache;
    void *mergedContent = nullptr;

    qCDebug(LogCache) << d->cacheBaseName << "read cache:" << ((d->options & (ClearCache | NoCache)) ? "no" : "yes")
                      << "/ write cache:" << ((d->options & NoCache) ? "no" : "yes");
    qCDebug(LogCache) << d->cacheBaseName << "reading:" << qPrintable(rawFilePaths.join(u", "_s));

    constexpr auto ShaType = QCryptographicHash::Sha256;
    static const int ShaSize = QCryptographicHash::hashLength(ShaType);

    if (!d->options.testFlag(NoCache) && !d->options.testFlag(ClearCache)) {
        std::unique_ptr<TrustedFile> cacheDevice;
        try {
            cacheDevice = SudoClient::instance()->openTrustedFile(QStandardPaths::CacheLocation,
                                                                  d->cacheBaseName + u".cache"_s);
        } catch (const Exception &) {
            // cache miss is the normal case; silent
        }
        if (cacheDevice) {
            try {
                if (cacheDevice->size() > (10 * 1024*1024))
                    throw Exception("cache is too big (> 10 MiB)");

                const QByteArray fileBytes = cacheDevice->readAll();
                if (fileBytes.size() < ShaSize)
                    throw Exception("cache is too small to even contain a checksum hash");

                QByteArray bytes = fileBytes.first(fileBytes.size() - ShaSize);
                if (QCryptographicHash::hash(bytes, ShaType) != fileBytes.last(ShaSize))
                    throw Exception("cache checksum hash mismatch");

                QDataStream ds(bytes);
                ds.setVersion(QDataStream::Qt_6_7);
                CacheHeader cacheHeader;
                ds >> cacheHeader;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("failed to read cache header");
                if (!cacheHeader.isValid(d->cacheBaseName, d->typeId, d->typeVersion))
                    throw Exception("failed to parse cache header");

                cache.resize(int(cacheHeader.m_entries));
                for (int i = 0; i < int(cacheHeader.m_entries); ++i) {
                    ConfigCacheEntry &ce = cache[i];
                    ds >> ce;
                    if (ce.m_content)
                        ce.m_content = loadFromCache(ds);
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

                        if ((rawFilePaths.at(i) != ce.m_filePath) || !ce.m_content)
                            cacheIsComplete = false;
                    }
                }
                d->cacheWasRead = true;

            } catch (const Exception &e) {
                qWarning(LogCache) << "Failed to read cache:" << e.what();
            }
        }
    } else if (d->options.testFlag(ClearCache)) {
        try {
            SudoClient::instance()->removeTrustedFile(QStandardPaths::CacheLocation,
                                                      d->cacheBaseName + u".cache"_s);
        } catch (const Exception &) {
            // best effort
        }
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
            for (const auto &c : std::as_const(cache)) {
                if ((c.m_filePath == rawFilePath) && c.m_content) {
                    ce = c;
                    found = true;
                    qCDebug(LogCache) << d->cacheBaseName << "found cache entry for" << c.m_filePath;
                    break;
                }
            }

            // if it's not yet cached, then add it to the list
            if (!found) {
                ce.m_filePath = rawFilePath;
                qCDebug(LogCache) << d->cacheBaseName << "missing cache entry for" << rawFilePath;
            }
        }
        cache = newCache;
    }

    // reads a single config file and calculates its hash - defined as lambda to be usable
    // both via QtConcurrent and via std:for_each
    auto readConfigFile = [&cacheIsComplete, &expectedByCanonical, this](ConfigCacheEntry &ce) {
        QFile file(ce.m_filePath);
        if (!file.open(QIODevice::ReadOnly))
            throw Exception("Failed to open file '%1' for reading.\n").arg(file.fileName());

        if (file.size() > 1024*1024)
            throw Exception("File '%1' is too big (> 1MB).\n").arg(file.fileName());

        ce.m_rawContent = file.readAll();
        preProcessSourceContent(ce.m_rawContent, ce.m_filePath);

        QByteArray checksum = QCryptographicHash::hash(ce.m_rawContent, QCryptographicHash::Sha256);
        ce.m_checksumMatches = (checksum == ce.m_checksum);
        ce.m_checksum = checksum;
        if (!ce.m_checksumMatches) {
            if (ce.m_content) {
                qWarning(LogCache) << "Failed to read Cache: cached file checksums do not match";
                destruct(ce.m_content);
                ce.m_content = nullptr;
            }
            cacheIsComplete = false;
        }

        if (auto it = expectedByCanonical.constFind(ce.m_filePath); it != expectedByCanonical.cend()) {
            if (checksum != it.value()) {
                qCWarning(LogCache) << "Source content digest mismatch for" << ce.m_filePath
                                    << "- expected:" << it.value().toHex()
                                    << "- actual:" << checksum.toHex();
                if (ce.m_content) {
                    destruct(ce.m_content);
                    ce.m_content = nullptr;
                }
                ce.m_rawContent.clear(); // prevent the next pass from parsing tampered bytes
                cacheIsComplete = false;
            }
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
            if (ce.m_content)
                return;
            if (ce.m_rawContent.isEmpty()) // cleared above (e.g. digest mismatch) - skip
                return;

            ++count;
            try {
                QBuffer buffer(&ce.m_rawContent);
                buffer.open(QIODevice::ReadOnly);
                ce.m_content = loadFromSource(&buffer, ce.m_filePath);
            } catch (const Exception &e) {
                if (d->options.testFlag(IgnoreBroken)) {
                    qCWarning(LogCache, "Could not parse file '%s': %s (file will be ignored)",
                              qPrintable(ce.m_filePath), qPrintable(e.errorString()));
                    ce.m_content = nullptr;
                } else {
                    throw Exception("Could not parse file '%1': %2")
                        .arg(ce.m_filePath).arg(e.errorString());
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
            for (const ConfigCacheEntry &ce : std::as_const(cache)) {
                if (ce.m_content) {
                    if (!mergedContent)
                        mergedContent = clone(ce.m_content);
                    else
                        merge(mergedContent, ce.m_content);
                }
            }
        }

        qCDebug(LogCache) << d->cacheBaseName << "parsing" << count.loadAcquire()
                          << "file(s) finished after" << (timer.nsecsElapsed() / 1000) << "usec";

        if (!d->options.testFlag(NoCache)) {
            // everything is parsed now, so we can write a new cache file

            try {
                auto writer = SudoClient::instance()->openTrustedSaveFile(QStandardPaths::CacheLocation,
                                                                          d->cacheBaseName + u".cache"_s);

                QByteArray bytes;
                QDataStream ds(&bytes, QIODevice::WriteOnly);
                ds.setVersion(QDataStream::Qt_6_7);
                CacheHeader cacheHeader;
                cacheHeader.m_baseName = d->cacheBaseName;
                cacheHeader.m_typeId = d->typeId;
                cacheHeader.m_typeVersion = d->typeVersion;
                cacheHeader.m_entries = quint32(cache.size());
                ds << cacheHeader;

                for (const ConfigCacheEntry &ce : std::as_const(cache)) {
                    ds << ce;
                    // qCDebug(LogCache) << "SAVING" << ce << ce.content;
                    if (ce.m_content)
                        saveToCache(ds, ce.m_content);
                }

                if (d->options & MergedResult) {
                    ds << bool(mergedContent);
                    if (mergedContent)
                        saveToCache(ds, mergedContent);
                }

                if (ds.status() != QDataStream::Ok)
                    throw Exception("error writing content");

                const QByteArray sha = QCryptographicHash::hash(bytes, ShaType);
                if (writer->write(bytes) != bytes.size() || writer->write(sha) != sha.size())
                    throw Exception("failed to write cache");
                writer->commit();

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
    for (auto &ce : std::as_const(d->cache))
        destruct(ce.m_content);
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

QT_END_NAMESPACE_AM
