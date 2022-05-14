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

#include "waylandqtamserverextension_p.h"

#include <QDataStream>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandResource>
#include <QtWaylandCompositor/QWaylandSurface>

#include <QtAppManCommon/logging.h>

QT_BEGIN_NAMESPACE_AM

WaylandQtAMServerExtension::WaylandQtAMServerExtension(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qtam_extension(compositor->display(), 1)
{ }

QVariantMap WaylandQtAMServerExtension::windowProperties(const QWaylandSurface *surface) const
{
    return m_windowProperties.value(surface);
}

void WaylandQtAMServerExtension::setWindowProperty(QWaylandSurface *surface, const QString &name, const QVariant &value)
{
    if (setWindowPropertyHelper(surface, name, value)) {
        QByteArray byteValue;
        QDataStream ds(&byteValue, QDataStream::WriteOnly);
        ds << value;

        Resource *target = resourceMap().value(surface->waylandClient());
        if (target) {
            qDebug(LogWaylandDebug) << "window property: server send" << surface << name << value;
            send_window_property_changed(target->handle, surface->resource(), name, byteValue);
        }
    }
}

bool WaylandQtAMServerExtension::setWindowPropertyHelper(QWaylandSurface *surface, const QString &name, const QVariant &value)
{
    auto it = m_windowProperties.find(surface);
    if ((it == m_windowProperties.end()) || (it.value().value(name) != value)) {
        if (it == m_windowProperties.end()) {
            m_windowProperties[surface].insert(name, value);
            connect(surface, &QWaylandSurface::surfaceDestroyed, this, [this, surface]() {
                m_windowProperties.remove(surface);
            });
        } else {
            it.value().insert(name, value);
        }
        emit windowPropertyChanged(surface, name, value);
        return true;
    }
    return false;
}

void WaylandQtAMServerExtension::qtam_extension_set_window_property(QtWaylandServer::qtam_extension::Resource *resource, wl_resource *surface_resource, const QString &name, wl_array *value)
{
    Q_UNUSED(resource);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_resource);
    const QByteArray byteValue(static_cast<const char *>(value->data), static_cast<int>(value->size));
    QDataStream ds(byteValue);
    QVariant variantValue;
    ds >> variantValue;

    qCDebug(LogWaylandDebug) << "window property: server receive" << surface << name << variantValue;
    setWindowPropertyHelper(surface, name, variantValue);
}

QT_END_NAMESPACE_AM

#include "moc_waylandqtamserverextension_p.cpp"
