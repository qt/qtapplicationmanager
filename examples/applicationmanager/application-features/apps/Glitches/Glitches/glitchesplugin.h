// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef GLITCHESPLUGIN_H
#define GLITCHESPLUGIN_H

#include <QQmlEngineExtensionPlugin>

extern void qml_register_types_Glitches();

class GlitchesPlugin : public QQmlEngineExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
    GlitchesPlugin(QObject *parent = nullptr) : QQmlEngineExtensionPlugin(parent)
    {
        volatile auto registration = &qml_register_types_Glitches;
        Q_UNUSED(registration);
    }
};

#endif  // GLITCHESPLUGIN_H
