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

#include <functional>

#include <QPair>
#include <QVector>
#include <QStringList>
#include <QVariant>
#include <QIODevice>
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
                        const char typeId[4] = nullptr, quint32 version = 0, Options options = None);
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
    Q_DISABLE_COPY(AbstractConfigCache)

    ConfigCachePrivate *d;
};

template <typename T, typename ADAPTOR = ConfigCacheAdaptor<T>> class ConfigCache : public AbstractConfigCache
{
public:
    using AbstractConfigCache::Option;
    using AbstractConfigCache::Options;

    ConfigCache(const QStringList &configFiles, const QString &cacheBaseName, const char typeId[4],
            qint32 typeVersion = 0, Options options = None)
        : AbstractConfigCache(configFiles, cacheBaseName, typeId, typeVersion, options)
    { }

    ~ConfigCache()
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

    ADAPTOR m_adaptor;

private:
    template <typename U = T> typename std::enable_if<std::is_copy_constructible<U>::value, void *>::type
    doClone(T *t)
    { return new T(*static_cast<T *>(t)); }
    template <typename U = T> typename std::enable_if<!std::is_copy_constructible<U>::value, void *>::type
    doClone(T *)
    { Q_ASSERT_X(false, "ConfigCache::clone", "trying to clone non-copy-constructible content"); return nullptr; }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractConfigCache::Options)

QT_END_NAMESPACE_AM
