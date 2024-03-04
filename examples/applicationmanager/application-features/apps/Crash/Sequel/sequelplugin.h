// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

// Needed for qmake only
#ifndef SEQUELPLUGIN_H
#define SEQUELPLUGIN_H

#include <QQmlEngineExtensionPlugin>

extern void qml_register_types_Sequel();

class SequelPlugin : public QQmlEngineExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
    SequelPlugin(QObject *parent = nullptr) : QQmlEngineExtensionPlugin(parent)
    {
        volatile auto registration = &qml_register_types_Sequel;
        Q_UNUSED(registration);
    }
};

#endif // SEQUELPLUGIN_H
