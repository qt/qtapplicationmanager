/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QMutex>
#include <QList>
#include <QSet>
#include <QScopedPointer>
#include <QThread>

#include <QtAppManManager/packagemanager.h>
#include <QtAppManApplication/packagedatabase.h>
#include <QtAppManManager/asynchronoustask.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

bool removeRecursiveHelper(const QString &path);

class PackageManagerPrivate
{
public:
    PackageDatabase *database = nullptr;
    QVector<Package *> packages;

    QMap<Package *, PackageInfo *> pendingPackageInfoUpdates;

#if defined(AM_DISABLE_INSTALLER)
    static constexpr
#endif
    bool disableInstaller = true;

    bool developmentMode = false;
    bool allowInstallationOfUnsignedPackages = false;
    bool userIdSeparation = false;
    uint minUserId = uint(-1);
    uint maxUserId = uint(-1);
    uint commonGroupId = uint(-1);

    QString installationPath;
    QString documentPath;

    QString error;

    QString hardwareId;
    QList<QByteArray> chainOfTrust;

    QList<AsynchronousTask *> incomingTaskList;     // incoming queue
    QList<AsynchronousTask *> installationTaskList; // installation jobs in state >= AwaitingAcknowledge
    AsynchronousTask *activeTask = nullptr;         // currently active

    QList<AsynchronousTask *> allTasks() const
    {
        QList<AsynchronousTask *> all = incomingTaskList;
        if (!installationTaskList.isEmpty())
            all += installationTaskList;
        if (activeTask)
            all += activeTask;
        return all;
    }
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
