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

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.ApplicationManager.ApplicationInterface")
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true)
    Q_PROPERTY(QVariantMap additionalConfiguration READ additionalConfiguration CONSTANT SCRIPTABLE true)

public:
    virtual QString applicationId() const = 0;
    virtual QVariantMap additionalConfiguration() const = 0;

#ifdef Q_QDOC
    Q_INVOKABLE Notification *createNotification();
#endif

signals:
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void memoryLowWarning();

    Q_SCRIPTABLE void openDocument(const QString &documentUrl);

protected:
    ApplicationInterface(QObject *parent)
        : QObject(parent)
    { }

private:
    Q_DISABLE_COPY(ApplicationInterface)
};
