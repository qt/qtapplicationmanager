// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>
#include <QString>


class PSConfiguration;
class PSPackages;
class PSHttpInterfacePrivate;


class PSHttpInterface : public QObject
{
    Q_OBJECT

public:
    PSHttpInterface(PSConfiguration *cfg, QObject *parent = nullptr);

    void listen();
    QString listenAddress() const;

    void setupRouting(PSPackages *packages);

private:
    PSHttpInterfacePrivate *d;
};
