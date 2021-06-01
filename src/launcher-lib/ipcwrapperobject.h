/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QDBusError>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)
QT_FORWARD_DECLARE_CLASS(QDBusConnection)

#ifdef interface
#undef interface
#endif

QT_BEGIN_NAMESPACE_AM

class IpcWrapperSignalRelay;

class IpcWrapperObject : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    IpcWrapperObject(const QString &service, const QString &path, const QString &interface,
                     const QDBusConnection &connection, QObject *parent = nullptr);

    ~IpcWrapperObject() override;

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

    static QVector<QMetaObject *> s_allMetaObjects;
};

QT_END_NAMESPACE_AM
