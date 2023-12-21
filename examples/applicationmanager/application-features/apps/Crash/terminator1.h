// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#pragma once

#include <QQmlEngine>


class Terminator1 : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    Terminator1(QObject *parent = nullptr);

    [[noreturn]] Q_INVOKABLE void abort() const;
    [[noreturn]] Q_INVOKABLE void throwUnhandledException() const;
    [[noreturn]] Q_INVOKABLE void exitGracefully() const;
};
