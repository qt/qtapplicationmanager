/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#pragma once

#include <QObject>
#include <QUrl>


class ApplicationInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT SCRIPTABLE true)

public:
    virtual QString applicationId() const = 0;

signals:
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void memoryLowWarning();

    Q_SCRIPTABLE void openDocument(const QString &url);

protected:
    ApplicationInterface(QObject *parent)
        : QObject(parent)
    { }

private:
    Q_DISABLE_COPY(ApplicationInterface)
};
