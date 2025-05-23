// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#ifndef PSHTTPINTERFACE_H
#define PSHTTPINTERFACE_H

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

#endif // PSHTTPINTERFACE_H
