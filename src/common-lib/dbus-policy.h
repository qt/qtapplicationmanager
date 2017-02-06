/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include <QtAppManCommon/global.h>
#if defined(QT_DBUS_LIB)
#  include <QDBusContext>
#endif
#include <functional>

QT_BEGIN_NAMESPACE_AM

// this is necessary to avoid a second <QDBusContext> forward include header
#if 0
#pragma qt_sync_stop_processing
QT_END_NAMESPACE_AM
#endif

struct DBusPolicy
{
    QList<uint> m_uids;
    QStringList m_executables;
    QStringList m_capabilities;
};

class Application;

QMap<QByteArray, DBusPolicy> parseDBusPolicy(const QVariantMap &yamlFragment);

#if !defined(QT_DBUS_LIB)

// evil hack, but QtDBus only works when directly deriving from QDBusContext
class QDBusContext
{
public:
    inline bool calledFromDBus() { return false; }
    inline void setDelayedReply(bool) const { }
    inline void sendErrorReply(const QString &, const QString & = QString()) const { }
};

#endif

bool checkDBusPolicy(const QDBusContext *dbusContext, const QMap<QByteArray, DBusPolicy> &dbusPolicy,
                     const QByteArray &function, const std::function<QStringList(qint64)> &pidToCapabilities);

QT_END_NAMESPACE_AM

