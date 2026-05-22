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
#include <QtAppManCommon/qml-utilities.h>

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
    if (!setWindowPropertyHelper(surface, name, value))
        return;
    Resource *target = resourceMap().value(surface->waylandClient());
    if (!target)
        return;

    QByteArray data;

    switch (target->version()) {
    case 1: {
        QDataStream ds(&data, QDataStream::WriteOnly);
        ds.setVersion(QDataStream::Qt_6_7);
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

    static const uint protocolSize = 24; // protocol overhead
    const uint nameSize = name.toUtf8().size();
    const uint dataSize = data.size();
    const uint messageSize = ((protocolSize + nameSize + dataSize + 3) & ~3u); // multiple of 4
    static const uint maxDefaultSize = 4096u; // default libwayland receive buffer size
    if (messageSize > maxDefaultSize) {
        qCCritical(LogWayland) << "Window property" << name << "is too large for the standard "
                                  "libwayland receive buffer size:" << messageSize << ">"
                               << maxDefaultSize << "bytes. Expect a crash.";
        // we send it anyway, because the user might have increased the buffer size on the client
        // side, but we cannot detect this.
    }

    qCDebug(LogWayland) << "Window property: server send" << surface << name << value;
    send_window_property_changed(target->handle, surface->resource(), name, data);
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
        ds.setVersion(QDataStream::Qt_6_7);
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
    // Enforce a nesting-level limit on inbound variants
    variantValue = sanitizeWindowPropertyValue(variantValue);

    qCDebug(LogWayland) << "window property: server receive" << surface << name << variantValue;
    setWindowPropertyHelper(surface, name, variantValue);
}

QT_END_NAMESPACE_AM

#include "moc_waylandqtamserverextension_p.cpp"
