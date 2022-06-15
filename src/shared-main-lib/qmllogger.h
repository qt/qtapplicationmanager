// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QObject>
#include <QQmlEngine>

QT_BEGIN_NAMESPACE_AM

class QmlLogger : public QObject
{
    Q_OBJECT

public:
    explicit QmlLogger(QQmlEngine *engine = nullptr);

private slots:
    void warnings(const QList<QQmlError> &list);
};

QT_END_NAMESPACE_AM
