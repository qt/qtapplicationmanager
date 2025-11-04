// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <tuple>

#include "watchdogconfiguration.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM


void WatchdogConfiguration::merge(const WatchdogConfiguration &other)
{
    auto mergeMs = [](std::chrono::milliseconds &into, std::chrono::milliseconds from) {
        if (from.count() != -1)
            into = from;
    };

    mergeMs(eventloop.checkInterval, other.eventloop.checkInterval);
    mergeMs(eventloop.warnTimeout, other.eventloop.warnTimeout);
    mergeMs(eventloop.killTimeout, other.eventloop.killTimeout);

    mergeMs(quickwindow.checkInterval, other.quickwindow.checkInterval);
    mergeMs(quickwindow.warnTimeout, other.quickwindow.warnTimeout);
    mergeMs(quickwindow.killTimeout, other.quickwindow.killTimeout);

    mergeMs(wayland.checkInterval, other.wayland.checkInterval);
    mergeMs(wayland.warnTimeout, other.wayland.warnTimeout);
    mergeMs(wayland.killTimeout, other.wayland.killTimeout);
}

bool WatchdogConfiguration::isValid(Type type) const
{
    return (eventloop.checkInterval > 0ms)
           || (quickwindow.checkInterval > 0ms)
           || ((type == SystemUI) && (wayland.checkInterval > 0ms));
}

QVariantMap WatchdogConfiguration::toMap(Type type) const
{
    static WatchdogConfiguration def;
    QVariantMap map;

    auto msToMap = [](QVariantMap &m, const QString &key, std::chrono::milliseconds ms, std::chrono::milliseconds defMs) {
        if (ms != defMs)
            m[key] = qint64(ms.count());
    };

    QVariantMap elMap;
    msToMap(elMap, u"checkInterval"_s, eventloop.checkInterval, def.eventloop.checkInterval);
    msToMap(elMap, u"warnTimeout"_s, eventloop.warnTimeout, def.eventloop.warnTimeout);
    msToMap(elMap, u"killTimeout"_s, eventloop.killTimeout, def.eventloop.killTimeout);
    map[u"eventloop"_s] = elMap;

    QVariantMap qwMap;
    msToMap(qwMap, u"checkInterval"_s, quickwindow.checkInterval, def.quickwindow.checkInterval);
    msToMap(qwMap, u"warnTimeout"_s, quickwindow.warnTimeout, def.quickwindow.warnTimeout);
    msToMap(qwMap, u"killTimeout"_s, quickwindow.killTimeout, def.quickwindow.killTimeout);
    map[u"quickwindow"_s] = qwMap;

    if (type == SystemUI) {
        QVariantMap wlMap;
        msToMap(wlMap, u"checkInterval"_s, wayland.checkInterval, def.wayland.checkInterval);
        msToMap(wlMap, u"warnTimeout"_s, wayland.warnTimeout, def.wayland.warnTimeout);
        msToMap(wlMap, u"killTimeout"_s, wayland.killTimeout, def.wayland.killTimeout);
        map[u"wayland"_s] = wlMap;
    }
    return map;
}

WatchdogConfiguration WatchdogConfiguration::fromMap(const QVariantMap &map, Type type)
{
    WatchdogConfiguration cfg;

    auto msFromMap = [](const QVariantMap &m, const QString &key, std::chrono::milliseconds &ms) {
        ms = std::chrono::milliseconds(m.value(key, qint64(ms.count())).toLongLong());
    };

    QVariantMap elMap = map.value(u"eventloop"_s).toMap();
    msFromMap(elMap, u"checkInterval"_s, cfg.eventloop.checkInterval);
    msFromMap(elMap, u"warnTimeout"_s, cfg.eventloop.warnTimeout);
    msFromMap(elMap, u"killTimeout"_s, cfg.eventloop.killTimeout);

    QVariantMap qwMap = map.value(u"quickwindow"_s).toMap();
    msFromMap(qwMap, u"checkInterval"_s, cfg.quickwindow.checkInterval);
    msFromMap(qwMap, u"warnTimeout"_s, cfg.quickwindow.warnTimeout);
    msFromMap(qwMap, u"killTimeout"_s, cfg.quickwindow.killTimeout);

    if (type == SystemUI) {
        QVariantMap wlMap = map.value(u"wayland"_s).toMap();
        msFromMap(wlMap, u"checkInterval"_s, cfg.wayland.checkInterval);
        msFromMap(wlMap, u"warnTimeout"_s, cfg.wayland.warnTimeout);
        msFromMap(wlMap, u"killTimeout"_s, cfg.wayland.killTimeout);
    }
    return cfg;
}

WatchdogConfiguration WatchdogConfiguration::fromYaml(YamlParser &yp, Type type)
{
    WatchdogConfiguration cfg;
    yp.parseFields({
        { "eventloop", false, YamlParser::Map, [&]() {
             yp.parseFields({
                 { "checkInterval", false, YamlParser::Scalar, [&]() {
                      cfg.eventloop.checkInterval = yp.parseDurationAsMSec(); } },
                 { "warnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.eventloop.warnTimeout = yp.parseDurationAsMSec(); } },
                 { "killTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.eventloop.killTimeout = yp.parseDurationAsMSec(); } },
             }); } },
        { "quickwindow", false, YamlParser::Map, [&]() {
             yp.parseFields({
                 { "checkInterval", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.checkInterval = yp.parseDurationAsMSec(); } },
                 { "warnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.warnTimeout = yp.parseDurationAsMSec(); } },
                 { "killTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.killTimeout = yp.parseDurationAsMSec(); } },
             }); } },
        { (type == SystemUI), "wayland", false, YamlParser::Map, [&]() {
             yp.parseFields({
                 { "checkInterval", false, YamlParser::Scalar, [&]() {
                      cfg.wayland.checkInterval = yp.parseDurationAsMSec(); } },
                 { "warnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.wayland.warnTimeout = yp.parseDurationAsMSec(); } },
                 { "killTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.wayland.killTimeout = yp.parseDurationAsMSec(); } },
             }); } }
    });
    return cfg;
}

bool WatchdogConfiguration::operator==(const WatchdogConfiguration &other) const
{
    return std::tie(eventloop.checkInterval,
                    eventloop.warnTimeout, eventloop.killTimeout,
                    quickwindow.checkInterval,
                    quickwindow.warnTimeout, quickwindow.killTimeout,
                    wayland.checkInterval,
                    wayland.warnTimeout, wayland.killTimeout) ==
           std::tie(other.eventloop.checkInterval,
                    other.eventloop.warnTimeout, other.eventloop.killTimeout,
                    other.quickwindow.checkInterval,
                    other.quickwindow.warnTimeout, other.quickwindow.killTimeout,
                    other.wayland.checkInterval,
                    other.wayland.warnTimeout, other.wayland.killTimeout);
}

bool WatchdogConfiguration::operator!=(const WatchdogConfiguration &other) const
{
    return !(*this == other);
}

static constexpr quint32 WatchdogConfigurationVersion = 1;

QDataStream &operator<<(QDataStream &ds, const WatchdogConfiguration &cfg)
{
    auto msOut = [](QDataStream &ds, std::chrono::milliseconds ms) {
        ds << qint64(ms.count());
    };

    ds << quint32(WatchdogConfigurationVersion);

    msOut(ds, cfg.eventloop.checkInterval);
    msOut(ds, cfg.eventloop.warnTimeout);
    msOut(ds, cfg.eventloop.killTimeout);
    msOut(ds, cfg.quickwindow.checkInterval);
    msOut(ds, cfg.quickwindow.warnTimeout);
    msOut(ds, cfg.quickwindow.killTimeout);
    msOut(ds, cfg.wayland.checkInterval);
    msOut(ds, cfg.wayland.warnTimeout);
    msOut(ds, cfg.wayland.killTimeout);
    return ds;
}

QDataStream &operator>>(QDataStream &ds, WatchdogConfiguration &cfg)
{
    auto msIn = [](QDataStream &ds, std::chrono::milliseconds &ms) {
        qint64 cnt;
        ds >> cnt;
        ms = std::chrono::milliseconds(cnt);
    };

    quint32 version = 0;
    ds >> version;

    if (version != WatchdogConfigurationVersion) {
        cfg = { };
        ds.setStatus(QDataStream::ReadCorruptData);
        return ds;
    }

    msIn(ds, cfg.eventloop.checkInterval);
    msIn(ds, cfg.eventloop.warnTimeout);
    msIn(ds, cfg.eventloop.killTimeout);
    msIn(ds, cfg.quickwindow.checkInterval);
    msIn(ds, cfg.quickwindow.warnTimeout);
    msIn(ds, cfg.quickwindow.killTimeout);
    msIn(ds, cfg.wayland.checkInterval);
    msIn(ds, cfg.wayland.warnTimeout);
    msIn(ds, cfg.wayland.killTimeout);
    return ds;
}

QT_END_NAMESPACE_AM
