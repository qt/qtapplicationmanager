// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "openglconfiguration.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

QVariantMap OpenGLConfiguration::toMap() const
{
    static OpenGLConfiguration def;
    QVariantMap map;
    if (desktopProfile != def.desktopProfile)
        map[u"desktopProfile"_s] = desktopProfile;
    if (esMajorVersion != def.esMajorVersion)
        map[u"esMajorVersion"_s] = esMajorVersion;
    if (esMinorVersion != def.esMinorVersion)
        map[u"esMinorVersion"_s] = esMinorVersion;
    return map;
}

OpenGLConfiguration OpenGLConfiguration::fromMap(const QVariantMap &map)
{
    OpenGLConfiguration cfg;
    cfg.desktopProfile = map.value(u"desktopProfile"_s, cfg.desktopProfile).toString();
    cfg.esMajorVersion = map.value(u"esMajorVersion"_s, cfg.esMajorVersion).toInt();
    cfg.esMinorVersion = map.value(u"esMinorVersion"_s, cfg.esMinorVersion).toInt();
    return cfg;
}

OpenGLConfiguration OpenGLConfiguration::fromYaml(YamlParser &yp)
{
    OpenGLConfiguration cfg;
    yp.parseFields({
        { "desktopProfile", false, YamlParser::Scalar, [&]() {
             cfg.desktopProfile = yp.parseString(); } },
        { "esMajorVersion", false, YamlParser::Scalar, [&]() {
             cfg.esMajorVersion = yp.parseInt(2); } },
        { "esMinorVersion", false, YamlParser::Scalar, [&]() {
             cfg.esMinorVersion = yp.parseInt(0); } }
    });
    return cfg;
}

OpenGLConfiguration::OpenGLConfiguration(const QString &profile, int major, int minor)
    : desktopProfile(profile), esMajorVersion(major), esMinorVersion(minor)
{ }

bool OpenGLConfiguration::operator==(const OpenGLConfiguration &other) const
{
    return (desktopProfile == other.desktopProfile)
           && (esMajorVersion == other.esMajorVersion)
           && (esMinorVersion == other.esMinorVersion);
}

bool OpenGLConfiguration::operator!=(const OpenGLConfiguration &other) const
{
    return !(*this == other);
}

QDataStream &operator<<(QDataStream &ds, const OpenGLConfiguration &cfg)
{
    ds << cfg.desktopProfile << cfg.esMajorVersion << cfg.esMinorVersion;
    return ds;
}

QDataStream &operator>>(QDataStream &ds, OpenGLConfiguration &cfg)
{
    ds >> cfg.desktopProfile >> cfg.esMajorVersion >> cfg.esMinorVersion;
    return ds;
}

QT_END_NAMESPACE_AM
