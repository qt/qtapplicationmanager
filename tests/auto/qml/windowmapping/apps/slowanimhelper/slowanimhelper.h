// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SLOWANIMHELPER_H
#define SLOWANIMHELPER_H

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

class SlowAnimHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit SlowAnimHelper(QObject *parent = nullptr);

    // the current animation speed modifier of this process' main-thread QUnifiedTimer
    // (1.0 normally, slowAnimationSpeed() when slow animations are enabled, -1 if not supported)
    Q_INVOKABLE qreal speedModifier() const;
};

#endif // SLOWANIMHELPER_H
