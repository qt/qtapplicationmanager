// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
    Q_UNUSED(resource)
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
