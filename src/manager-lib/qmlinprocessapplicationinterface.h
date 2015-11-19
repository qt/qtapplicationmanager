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

#include "applicationinterface.h"
#include "notification.h"

class QmlInProcessRuntime;

class QmlInProcessNotification : public Notification
{
public:
    QmlInProcessNotification(QObject *parent = 0, ConstructionMode mode = Declarative);

    void componentComplete() override;

protected:
    uint libnotifyShow() override;
    void libnotifyClose() override;

private:
    ConstructionMode m_mode;
    QString m_appId;

    friend class QmlInProcessApplicationInterface;
};

class QmlInProcessApplicationInterface : public ApplicationInterface
{
    Q_OBJECT

public:
    explicit QmlInProcessApplicationInterface(QmlInProcessRuntime *runtime = 0);

    QString applicationId() const override;

    Q_INVOKABLE Notification *createNotification();

private:
    QmlInProcessRuntime *m_runtime;
    //QList<QPointer<QmlInProcessNotification> > m_allNotifications;

    friend class QmlInProcessRuntime;
};
