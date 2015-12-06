/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QThread>

#include "applicationinstaller.h"
#include "sudo.h"
#include "exception.h"
#include "dbus-utilities.h"
#include "global.h"


bool removeRecursiveHelper(const QString &path);


class ApplicationInstallerPrivate
{
public:
    bool developmentMode = false;
    bool allowInstallationOfUnsignedPackages = false;
    bool userIdSeparation = false;
    uint minUserId = uint(-1);
    uint maxUserId = uint(-1);
    uint commonGroupId = uint(-1);

    QDir manifestDir;
    QDir imageMountDir;
    QVector<InstallationLocation> installationLocations;
    InstallationLocation invalidInstallationLocation;

    QString error;

    QList<QByteArray> chainOfTrust;

    QQueue<AsynchronousTask *> taskQueue;
    AsynchronousTask *activeTask = nullptr;

    QMutex activationLock;
    QMap<QString, QString> activatedPackages; // id -> installationPath

    QMap<QByteArray, DBusPolicy> dbusPolicy;
};
