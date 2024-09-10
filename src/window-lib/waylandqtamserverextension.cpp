// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "waylandqtamserverextension_p.h"

#include <QDataStream>
#include <QCborValue>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandResource>
#include <QtWaylandCompositor/QWaylandSurface>

#include <QtAppManCommon/logging.h>

QT_BEGIN_NAMESPACE_AM

WaylandQtAMServerExtension::WaylandQtAMServerExtension(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qtam_extension(compositor->display(), 2)
{ }

QVariantMap WaylandQtAMServerExtension::windowProperties(const QWaylandSurface *surface) const
{
    return m_windowProperties.value(surface);
}

void WaylandQtAMServerExtension::setWindowProperty(QWaylandSurface *surface, const QString &name, const QVariant &value)
{
    if (setWindowPropertyHelper(surface, name, value)) {
        if (Resource *target = resourceMap().value(surface->waylandClient())) {
            QByteArray data;

            switch (target->version()) {
            case 1: {
                QDataStream ds(&data, QDataStream::WriteOnly);
                ds << value;
                break;
            }
            case 2:
                data = QCborValue::fromVariant(value).toCbor();
                break;
            default:
                qCWarning(LogWayland) << "Unsupported qtam_extension version:" << target->version();
                return;
            }

            qCDebug(LogWayland) << "window property: server send" << surface << name << value;
            send_window_property_changed(target->handle, surface->resource(), name, data);
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
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_resource);
    const auto data = QByteArray::fromRawData(static_cast<const char *>(value->data), qsizetype(value->size));
    QVariant variantValue;

    switch (resource->version()) {
    case 1: {
        QDataStream ds(data);
        ds >> variantValue;
        break;
    }
    case 2:
        variantValue = QCborValue::fromCbor(data).toVariant();
        break;
    default:
        qCWarning(LogWayland) << "Unsupported qtam_extension version:" << resource->version();
        return;
    }
    qCDebug(LogWayland) << "window property: server receive" << surface << name << variantValue;
    setWindowPropertyHelper(surface, name, variantValue);
}

QT_END_NAMESPACE_AM

#include "moc_waylandqtamserverextension_p.cpp"
