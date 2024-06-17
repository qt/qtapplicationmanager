// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef OPENGLCONFIGURATION_H
#define OPENGLCONFIGURATION_H

#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/qtyaml.h>

QT_BEGIN_NAMESPACE_AM

class OpenGLConfiguration
{
public:
    QString desktopProfile;
    int esMajorVersion = -1;
    int esMinorVersion = -1;

    QVariantMap toMap() const;
    static OpenGLConfiguration fromMap(const QVariantMap &map);
    static OpenGLConfiguration fromYaml(YamlParser &yp);

    OpenGLConfiguration() = default;
    OpenGLConfiguration(const OpenGLConfiguration &copy) = default;
    OpenGLConfiguration &operator=(const OpenGLConfiguration &other) = default;
    explicit OpenGLConfiguration(const QString &profile, int major, int minor);

    bool operator==(const OpenGLConfiguration &other) const;
    bool operator!=(const OpenGLConfiguration &other) const;
};

QDataStream &operator<<(QDataStream &ds, const OpenGLConfiguration &cfg);
QDataStream &operator>>(QDataStream &ds, OpenGLConfiguration &cfg);

QT_END_NAMESPACE_AM

#endif // OPENGLCONFIGURATION_H
