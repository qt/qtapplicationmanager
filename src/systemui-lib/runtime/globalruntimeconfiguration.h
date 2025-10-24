// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef GLOBALRUNTIMECONFIGURATION_H
#define GLOBALRUNTIMECONFIGURATION_H

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>
#include <QtAppManCommon/openglconfiguration.h>
#include <QtAppManCommon/watchdogconfiguration.h>

QT_BEGIN_NAMESPACE_AM

class Q_APPMANSYSTEMUI_EXPORT GlobalRuntimeConfiguration
{
public:
    OpenGLConfiguration openGLConfiguration;
    bool watchdogDisabled = false;
    WatchdogConfiguration watchdogConfiguration;
    QStringList iconThemeSearchPaths;
    QString iconThemeName;
    QVariantMap systemPropertiesForThirdPartyApps;
    QVariantMap systemPropertiesForBuiltInApps;

    static GlobalRuntimeConfiguration &instance();
};

QT_END_NAMESPACE_AM

#endif // GLOBALRUNTIMECONFIGURATION_H
