// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WATCHDOGCONFIGURATION_H
#define WATCHDOGCONFIGURATION_H

#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QDataStream>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/qtyaml.h>

QT_BEGIN_NAMESPACE_AM

class WatchdogConfiguration
{
public:
    enum Type { SystemUI, Application };

    struct {
        std::chrono::milliseconds checkInterval { -1 };
        std::chrono::milliseconds warnTimeout { -1 };
        std::chrono::milliseconds killTimeout { -1 };
    } eventloop;
    struct {
        std::chrono::milliseconds checkInterval { -1 };
        std::chrono::milliseconds warnTimeout { -1 };
        std::chrono::milliseconds killTimeout { -1 };
    } quickwindow;
    struct {
        std::chrono::milliseconds checkInterval { -1 };
        std::chrono::milliseconds warnTimeout { -1 };
        std::chrono::milliseconds killTimeout { -1 };
    } wayland;

    void merge(const WatchdogConfiguration &other);
    bool isValid(Type type) const;

    QVariantMap toMap(Type type) const;
    static WatchdogConfiguration fromMap(const QVariantMap &map, Type type);
    static WatchdogConfiguration fromYaml(YamlParser &yp, Type type);

    WatchdogConfiguration() = default;
    WatchdogConfiguration(const WatchdogConfiguration &copy) = default;
    WatchdogConfiguration &operator=(const WatchdogConfiguration &other) = default;

    bool operator==(const WatchdogConfiguration &other) const;
    bool operator!=(const WatchdogConfiguration &other) const;
};

QDataStream &operator<<(QDataStream &ds, const WatchdogConfiguration &cfg);
QDataStream &operator>>(QDataStream &ds, WatchdogConfiguration &cfg);

QT_END_NAMESPACE_AM

#endif // WATCHDOGCONFIGURATION_H
