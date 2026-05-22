// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QML_UTILITIES_H
#define QML_UTILITIES_H

#include <QtAppManCommon/qtappmancommonglobal.h>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

// This macro is necessary to force the linker to include the code that registers the types. We
// are statically linking, so any unreferenced symbol is not linked in at all.
#define AM_QML_REGISTER_TYPES(URI_UNDERSCORE) \
    QT_BEGIN_NAMESPACE \
    extern void qml_register_types_##URI_UNDERSCORE(); \
    auto qtam_static_qml_register_types_##URI_UNDERSCORE = &qml_register_types_##URI_UNDERSCORE; \
    QT_END_NAMESPACE


QT_BEGIN_NAMESPACE_AM

// recursively resolve QJSValues and limit nesting level
Q_APPMANCOMMON_EXPORT QVariant sanitizeWindowPropertyValue(const QVariant &variant);

Q_APPMANCOMMON_EXPORT bool tagQmlContext(QQmlContext *context, const QVariant &value);
Q_APPMANCOMMON_EXPORT QVariant findTaggedQmlContext(QObject *object);

Q_APPMANCOMMON_EXPORT bool ensureCurrentContextIsSystemUI(QObject *object);
Q_APPMANCOMMON_EXPORT bool ensureCurrentContextIsInProcessApplication(QObject *object);

QT_END_NAMESPACE_AM

#endif // QML_UTILITIES_H
