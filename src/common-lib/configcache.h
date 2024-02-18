// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>
#include <array>

#include <QtCore/QPair>
#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QIODevice>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class ConfigCachePrivate;

template <typename T> class ConfigCacheAdaptor
{
public:
    T *loadFromSource(QIODevice *source, const QString &fileName);
    T *loadFromCache(QDataStream &ds);
    void saveToCache(QDataStream &ds, const T *t);
    void merge(T *to, const T *from) { *to = *from; }
};

class AbstractConfigCache
{
public:
    enum Option {
        None          = 0x0,
        MergedResult  = 0x1,
        NoCache       = 0x2,
        ClearCache    = 0x4,
        IgnoreBroken  = 0x8,
    };
    Q_DECLARE_FLAGS(Options, Option)

    AbstractConfigCache(const QStringList &configFiles, const QString &cacheBaseName,
                        std::array<char, 4> typeId, quint32 version = 0, Options options = None);
    virtual ~AbstractConfigCache();

    virtual void parse();

    void *takeMergedResult() const;
    void *takeResult(int index) const;
    void *takeResult(const QString &rawFile) const;

    void clear();

    // mainly for debugging and auto tests
    bool parseReadFromCache() const;
    bool parseWroteToCache() const;
    QString cacheFilePath() const;

protected:
    virtual void *loadFromSource(QIODevice *source, const QString &fileName) = 0;
    virtual void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName) = 0;
    virtual void *loadFromCache(QDataStream &ds) = 0;
    virtual void saveToCache(QDataStream &ds, const void *t) = 0;
    virtual void merge(void *to, const void *from) = 0;
    virtual void *clone(void *t) = 0;
    virtual void destruct(void *t) = 0;

private:
    Q_DISABLE_COPY_MOVE(AbstractConfigCache)

    ConfigCachePrivate *d;
};

template <typename T, typename ADAPTOR = ConfigCacheAdaptor<T>> class ConfigCache : public AbstractConfigCache
{
public:
    using AbstractConfigCache::Option;
    using AbstractConfigCache::Options;

    ConfigCache(const QStringList &configFiles, const QString &cacheBaseName, std::array<char, 4> typeId,
                quint32 typeVersion = 0, Options options = None)
        : AbstractConfigCache(configFiles, cacheBaseName, typeId, typeVersion, options)
        , m_adaptor()
    { }

    ~ConfigCache() override
    {
        clear();
    }

    void parse() override
    {
        AbstractConfigCache::parse();
    }

    T *takeMergedResult() const
    {
        return static_cast<T *>(AbstractConfigCache::takeMergedResult());
    }
    T *takeResult(int index) const
    {
        return static_cast<T *>(AbstractConfigCache::takeResult(index));
    }
    T *takeResult(const QString &yamlFile) const
    {
        return static_cast<T *>(AbstractConfigCache::takeResult(yamlFile));
    }

protected:
    void *loadFromSource(QIODevice *source, const QString &fileName) override
    { return m_adaptor.loadFromSource(source, fileName); }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName) override
    { m_adaptor.preProcessSourceContent(sourceContent, fileName); }
    void *loadFromCache(QDataStream &ds) override
    { return m_adaptor.loadFromCache(ds); }
    void saveToCache(QDataStream &ds, const void *t) override
    { m_adaptor.saveToCache(ds, static_cast<const T *>(t)); }
    void merge(void *to, const void *from) override
    { m_adaptor.merge(static_cast<T *>(to), static_cast<const T *>(from)); }
    void *clone(void *t) override
    { return doClone(static_cast<T *>(t)); }
    void destruct(void *t) override
    { delete static_cast<T *>(t); }

private:
    ADAPTOR m_adaptor;

    template <typename U = T> typename std::enable_if<std::is_copy_constructible<U>::value, void *>::type
    doClone(T *t)
    { return new T(*static_cast<T *>(t)); }
    template <typename U = T> typename std::enable_if<!std::is_copy_constructible<U>::value, void *>::type
    doClone(T *)
    { Q_ASSERT_X(false, "ConfigCache::clone", "trying to clone non-copy-constructible content"); return nullptr; }

    Q_DISABLE_COPY_MOVE(ConfigCache)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractConfigCache::Options)

QT_END_NAMESPACE_AM
