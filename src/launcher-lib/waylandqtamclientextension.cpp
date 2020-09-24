/****************************************************************************
**
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

#include "waylandqtamclientextension_p.h"

#include <QWindow>
#include <QGuiApplication>
#include <QEvent>
#include <QExposeEvent>
#include <qpa/qplatformnativeinterface.h>

#include <QtAppManCommon/logging.h>

QT_BEGIN_NAMESPACE_AM

WaylandQtAMClientExtension::WaylandQtAMClientExtension()
    : QWaylandClientExtensionTemplate(1)
{
    qApp->installEventFilter(this);
}

WaylandQtAMClientExtension::~WaylandQtAMClientExtension()
{
    qApp->removeEventFilter(this);
}

bool WaylandQtAMClientExtension::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Expose) {
        if (!isActive()) {
            qCWarning(LogGraphics) << "WaylandQtAMClientExtension is not active";
            return false;
        }

        if (static_cast<QExposeEvent *>(e)->region().isNull())
            return false;

        QWindow *window = qobject_cast<QWindow *>(o);
        Q_ASSERT(window);

        if (m_windowToSurface.contains(window))
            return false;

        auto surface = static_cast<struct ::wl_surface *>
                       (QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", window));
        m_windowToSurface.insert(window, surface);
        const QVariantMap wp = windowProperties(window);
        for (auto it = wp.cbegin(); it != wp.cend(); ++it)
            sendPropertyToServer(surface, it.key(), it.value());
    } else if (e->type() == QEvent::Hide) {
        m_windowToSurface.remove(qobject_cast<QWindow *>(o));
    }

    return false;
}

QVariantMap WaylandQtAMClientExtension::windowProperties(QWindow *window) const
{
    return m_windowProperties.value(window);
}

void WaylandQtAMClientExtension::sendPropertyToServer(struct ::wl_surface *surface, const QString &name,
                                                      const QVariant &value)
{
    QByteArray byteValue;
    QDataStream ds(&byteValue, QIODevice::WriteOnly);
    ds << value;
    qCDebug(LogWaylandDebug) << "CLIENT >>prop>>" << surface << name << value;
    set_window_property(surface, name, byteValue);
}

void WaylandQtAMClientExtension::setWindowProperty(QWindow *window, const QString &name, const QVariant &value)
{
    if (setWindowPropertyHelper(window, name, value) && m_windowToSurface.contains(window)) {
        auto surface = static_cast<struct ::wl_surface *>
                       (QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", window));
        if (surface)
            sendPropertyToServer(surface, name, value);
    }
}

bool WaylandQtAMClientExtension::setWindowPropertyHelper(QWindow *window, const QString &name, const QVariant &value)
{
    auto it = m_windowProperties.find(window);
    if ((it == m_windowProperties.end()) || (it.value().value(name) != value)) {
        if (it == m_windowProperties.end())
            m_windowProperties[window].insert(name, value);
        else
            it.value().insert(name, value);

        emit windowPropertyChanged(window, name, value);
        return true;
    }
    return false;
}

void WaylandQtAMClientExtension::clearWindowPropertyCache(QWindow *window)
{
    m_windowProperties.remove(window);
}

void WaylandQtAMClientExtension::qtam_extension_window_property_changed(wl_surface *surface, const QString &name,
                                                                        wl_array *value)
{
    const QByteArray data = QByteArray::fromRawData(static_cast<char *>(value->data), int(value->size));
    QDataStream ds(data);
    QVariant variantValue;
    ds >> variantValue;

    QWindow *window = m_windowToSurface.key(surface);
    qCDebug(LogWaylandDebug) << "CLIENT <<prop<<" << window << name << variantValue;
    if (!window)
        return;

    setWindowPropertyHelper(window, name, variantValue);
}

QT_END_NAMESPACE_AM

#include "moc_waylandqtamclientextension_p.cpp"
