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
    mergeMs(quickwindow.syncWarnTimeout, other.quickwindow.syncWarnTimeout);
    mergeMs(quickwindow.syncKillTimeout, other.quickwindow.syncKillTimeout);
    mergeMs(quickwindow.renderWarnTimeout, other.quickwindow.renderWarnTimeout);
    mergeMs(quickwindow.renderKillTimeout, other.quickwindow.renderKillTimeout);
    mergeMs(quickwindow.swapWarnTimeout, other.quickwindow.swapWarnTimeout);
    mergeMs(quickwindow.swapKillTimeout, other.quickwindow.swapKillTimeout);

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
    msToMap(qwMap, u"syncWarnTimeout"_s, quickwindow.syncWarnTimeout, def.quickwindow.syncWarnTimeout);
    msToMap(qwMap, u"syncKillTimeout"_s, quickwindow.syncKillTimeout, def.quickwindow.syncKillTimeout);
    msToMap(qwMap, u"renderWarnTimeout"_s, quickwindow.renderWarnTimeout, def.quickwindow.renderWarnTimeout);
    msToMap(qwMap, u"renderKillTimeout"_s, quickwindow.renderKillTimeout, def.quickwindow.renderKillTimeout);
    msToMap(qwMap, u"swapWarnTimeout"_s, quickwindow.swapWarnTimeout, def.quickwindow.swapWarnTimeout);
    msToMap(qwMap, u"swapKillTimeout"_s, quickwindow.swapKillTimeout, def.quickwindow.swapKillTimeout);
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
    msFromMap(qwMap, u"syncWarnTimeout"_s, cfg.quickwindow.syncWarnTimeout);
    msFromMap(qwMap, u"syncKillTimeout"_s, cfg.quickwindow.syncKillTimeout);
    msFromMap(qwMap, u"renderWarnTimeout"_s, cfg.quickwindow.renderWarnTimeout);
    msFromMap(qwMap, u"renderKillTimeout"_s, cfg.quickwindow.renderKillTimeout);
    msFromMap(qwMap, u"swapWarnTimeout"_s, cfg.quickwindow.swapWarnTimeout);
    msFromMap(qwMap, u"swapKillTimeout"_s, cfg.quickwindow.swapKillTimeout);

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
                 { "syncWarnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.syncWarnTimeout = yp.parseDurationAsMSec(); } },
                 { "syncKillTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.syncKillTimeout = yp.parseDurationAsMSec(); } },
                 { "renderWarnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.renderWarnTimeout = yp.parseDurationAsMSec(); } },
                 { "renderKillTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.renderKillTimeout = yp.parseDurationAsMSec(); } },
                 { "swapWarnTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.swapWarnTimeout = yp.parseDurationAsMSec(); } },
                 { "swapKillTimeout", false, YamlParser::Scalar, [&]() {
                      cfg.quickwindow.swapKillTimeout = yp.parseDurationAsMSec(); } },
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
                    quickwindow.syncWarnTimeout, quickwindow.syncKillTimeout,
                    quickwindow.renderWarnTimeout, quickwindow.renderKillTimeout,
                    quickwindow.swapWarnTimeout, quickwindow.swapKillTimeout,
                    wayland.checkInterval,
                    wayland.warnTimeout, wayland.killTimeout) ==
           std::tie(other.eventloop.checkInterval,
                    other.eventloop.warnTimeout, other.eventloop.killTimeout,
                    other.quickwindow.checkInterval,
                    other.quickwindow.syncWarnTimeout, other.quickwindow.syncKillTimeout,
                    other.quickwindow.renderWarnTimeout, other.quickwindow.renderKillTimeout,
                    other.quickwindow.swapWarnTimeout, other.quickwindow.swapKillTimeout,
                    other.wayland.checkInterval,
                    other.wayland.warnTimeout, other.wayland.killTimeout);
}

bool WatchdogConfiguration::operator!=(const WatchdogConfiguration &other) const
{
    return !(*this == other);
}

QDataStream &operator<<(QDataStream &ds, const WatchdogConfiguration &cfg)
{
    auto msOut = [](QDataStream &ds, std::chrono::milliseconds ms) {
        ds << qint64(ms.count());
    };

    msOut(ds, cfg.eventloop.checkInterval);
    msOut(ds, cfg.eventloop.warnTimeout);
    msOut(ds, cfg.eventloop.killTimeout);
    msOut(ds, cfg.quickwindow.checkInterval);
    msOut(ds, cfg.quickwindow.syncWarnTimeout);
    msOut(ds, cfg.quickwindow.syncKillTimeout);
    msOut(ds, cfg.quickwindow.renderWarnTimeout);
    msOut(ds, cfg.quickwindow.renderKillTimeout);
    msOut(ds, cfg.quickwindow.swapWarnTimeout);
    msOut(ds, cfg.quickwindow.swapKillTimeout);
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

    msIn(ds, cfg.eventloop.checkInterval);
    msIn(ds, cfg.eventloop.warnTimeout);
    msIn(ds, cfg.eventloop.killTimeout);
    msIn(ds, cfg.quickwindow.checkInterval);
    msIn(ds, cfg.quickwindow.syncWarnTimeout);
    msIn(ds, cfg.quickwindow.syncKillTimeout);
    msIn(ds, cfg.quickwindow.renderWarnTimeout);
    msIn(ds, cfg.quickwindow.renderKillTimeout);
    msIn(ds, cfg.quickwindow.swapWarnTimeout);
    msIn(ds, cfg.quickwindow.swapKillTimeout);
    msIn(ds, cfg.wayland.checkInterval);
    msIn(ds, cfg.wayland.warnTimeout);
    msIn(ds, cfg.wayland.killTimeout);
    return ds;
}

QT_END_NAMESPACE_AM
