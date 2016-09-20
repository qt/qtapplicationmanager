/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QObject>
#include <qqml.h>
#if defined(QT_DBUS_LIB)
#  include <QDBusConnection>
#endif
#include "global.h"

AM_BEGIN_NAMESPACE

class Application;
class ApplicationIPCInterfaceAttached;
class IpcProxyObject;

class ApplicationIPCInterface : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationIPCInterface(QObject *parent = nullptr);

    QString interfaceName() const;
    QString pathName() const;
    bool isValidForApplication(const Application *app) const;

#if defined(QT_DBUS_LIB)
    bool dbusRegister(const Application *app, QDBusConnection connection, const QString &debugPathPrefix = QString());
    bool dbusUnregister(QDBusConnection connection);
#endif

public:
    static ApplicationIPCInterfaceAttached *qmlAttachedProperties(QObject *object);

private:
    IpcProxyObject *m_ipcProxy = nullptr;

    friend class ApplicationIPCManager;
};

AM_END_NAMESPACE

QML_DECLARE_TYPEINFO(AM_PREPEND_NAMESPACE(ApplicationIPCInterface), QML_HAS_ATTACHED_PROPERTIES)
