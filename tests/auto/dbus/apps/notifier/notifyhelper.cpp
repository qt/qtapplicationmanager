// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include "notifyhelper.h"

using namespace Qt::StringLiterals;

NotifyHelper::NotifyHelper(QObject *parent)
    : QObject(parent)
{}

uint NotifyHelper::notify(const QString &appName, const QString &summary)
{
    // a blocking call is fine here: the Notifications service lives in the (separate) System-UI
    // process, so there is no same-process dispatch deadlock
    QDBusInterface iface(u"org.freedesktop.Notifications"_s, u"/org/freedesktop/Notifications"_s,
                         u"org.freedesktop.Notifications"_s, QDBusConnection::sessionBus());

    QDBusReply<uint> reply = iface.call(u"Notify"_s,
                                        appName,          // app_name
                                        0u,               // replaces_id
                                        QString(),        // app_icon
                                        summary,          // summary
                                        QString(),        // body
                                        QStringList(),    // actions
                                        QVariantMap(),    // hints
                                        -1);              // timeout
    return reply.isValid() ? reply.value() : 0;
}
