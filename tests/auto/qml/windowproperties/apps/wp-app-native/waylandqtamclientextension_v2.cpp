// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "waylandqtamclientextension_v2_p.h"

#include <QWindow>
#include <QGuiApplication>
#include <QEvent>
#include <QExposeEvent>
#include <qpa/qplatformnativeinterface.h>
#include <QJSValue>
#include <QCborValue>


// This is a copy of the built-in class WaylandQtAMClientExtension (version 1). Not all
// functionality is actually used for the test, but it makes it easier to keep the code in sync
// this way, as they share ~90% of the implementation.

WaylandQtAMClientExtensionV2::WaylandQtAMClientExtensionV2()
    : QWaylandClientExtensionTemplate(2)
{
    qApp->installEventFilter(this);
}

WaylandQtAMClientExtensionV2::~WaylandQtAMClientExtensionV2()
{
    qApp->removeEventFilter(this);
}

bool WaylandQtAMClientExtensionV2::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Expose) {
        if (!isActive()) {
            qWarning() << "WaylandQtAMClientExtensionV2 is not active";
        } else {
            QWindow *window = qobject_cast<QWindow *>(o);
            Q_ASSERT(window);

            // we're only interested in the first expose to setup our mapping
            if (!m_windowToSurface.contains(window)) {
                auto surface = static_cast<struct ::wl_surface *>
                    (QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", window));
                if (surface) {
                    m_windowToSurface.insert(window, surface);
                    const QVariantMap wp = windowProperties(window);
                    for (auto it = wp.cbegin(); it != wp.cend(); ++it)
                        sendPropertyToServer(surface, it.key(), it.value());
                }
                // pointers can be reused, so we have to remove the old mappings
                connect(window, &QObject::destroyed, this, [this, window]() {
                    m_windowToSurface.remove(window);
                    m_windowProperties.remove(window);
                });
            }
        }
    } else if (e->type() == QEvent::Hide) {
        m_windowToSurface.remove(qobject_cast<QWindow *>(o));
    }

    return QWaylandClientExtensionTemplate<WaylandQtAMClientExtensionV2>::eventFilter(o, e);
}

QVariantMap WaylandQtAMClientExtensionV2::windowProperties(QWindow *window) const
{
    return m_windowProperties.value(window);
}

void WaylandQtAMClientExtensionV2::sendPropertyToServer(struct ::wl_surface *surface, const QString &name,
                                                        const QVariant &value)
{
    if (int(qtam_extension::version()) != QWaylandClientExtension::version()) {
        qWarning() << "Unsupported qtam_extension version:" << qtam_extension::version();
        return;
    }
    const QByteArray data = QCborValue::fromVariant(value).toCbor();
    set_window_property(surface, name, data);
}

bool WaylandQtAMClientExtensionV2::setWindowProperty(QWindow *window, const QString &name, const QVariant &value)
{
    if (setWindowPropertyHelper(window, name, value) && m_windowToSurface.contains(window)) {
        auto surface = static_cast<struct ::wl_surface *>
                       (QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", window));
        if (surface) {
            sendPropertyToServer(surface, name, value);
            return true;
        }
    }
    return false;
}

bool WaylandQtAMClientExtensionV2::setWindowPropertyHelper(QWindow *window, const QString &name, const QVariant &value)
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

void WaylandQtAMClientExtensionV2::clearWindowPropertyCache(QWindow *window)
{
    m_windowProperties.remove(window);
}

void WaylandQtAMClientExtensionV2::qtam_extension_window_property_changed(wl_surface *surface, const QString &name,
                                                                          wl_array *value)
{
    if (int(qtam_extension::version()) != QWaylandClientExtension::version()) {
        qWarning() << "Unsupported qtam_extension version:" << qtam_extension::version();
        return;
    }

    if (QWindow *window = m_windowToSurface.key(surface)) {
        const auto data = QByteArray::fromRawData(static_cast<const char *>(value->data), qsizetype(value->size));
        const QVariant variantValue = QCborValue::fromCbor(data).toVariant();
        setWindowPropertyHelper(window, name, variantValue);
    }
}

#include "moc_waylandqtamclientextension_v2_p.cpp"
