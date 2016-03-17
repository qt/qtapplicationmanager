/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
#include <QDBusError>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)
QT_FORWARD_DECLARE_CLASS(QDBusConnection)
class IpcWrapperSignalRelay;


class IpcWrapperObject : public QObject
{
public:
    IpcWrapperObject(const QString &service, const QString &path, const QString &interface,
                     const QDBusConnection &connection, QObject *parent = nullptr);

    ~IpcWrapperObject();

    bool isDBusValid() const;
    QDBusError lastDBusError() const;

    const QMetaObject *metaObject() const override;
    int qt_metacall(QMetaObject::Call _c, int _id, void **_a) override;

    // this should be a slot, but we do not want to "pollute" the QML namespace,
    // so we are relaying the signal via IpcWrapperHelper
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    QMetaObject *m_metaObject;
    IpcWrapperSignalRelay *m_wrapperHelper;
    QDBusInterface *m_dbusInterface;
};
