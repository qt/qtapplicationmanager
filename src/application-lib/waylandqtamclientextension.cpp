// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
            qCWarning(LogWayland) << "WaylandQtAMClientExtension is not active";
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

    return QWaylandClientExtensionTemplate<WaylandQtAMClientExtension>::eventFilter(o, e);
}

QVariantMap WaylandQtAMClientExtension::windowProperties(QWindow *window) const
{
    return m_windowProperties.value(window);
}

void WaylandQtAMClientExtension::sendPropertyToServer(struct ::wl_surface *surface, const QString &name,
                                                      const QVariant &value)
{
    if (int(qtam_extension::version()) != QWaylandClientExtension::version()) {
        qCWarning(LogWayland) << "Unsupported qtam_extension version:" << qtam_extension::version();
        return;
    }
    QByteArray data;
    QDataStream ds(&data, QDataStream::WriteOnly);
    ds.setVersion(QDataStream::Qt_6_7);
    ds << value;

    static const uint protocolSize = 24u; // protocol overhead
    const uint nameSize = name.toUtf8().size();
    const uint dataSize = data.size();
    const uint messageSize = ((protocolSize + nameSize + dataSize + 3) & ~3u); // multiple of 4
    static const uint maxDefaultSize = 4096u; // default libwayland receive buffer size
    if (messageSize > maxDefaultSize) {
        qCCritical(LogWayland) << "Window property" << name << "is too large for the standard "
                                  "libwayland receive buffer size:" << messageSize << ">"
                               << maxDefaultSize << "bytes. Expect a crash.";
        // we send it anyway, because the user might have increased the buffer size on the server
        // side, but we cannot detect this.
    }

    qCDebug(LogWayland) << "Window property: client send:" << surface << name << value;
    set_window_property(surface, name, data);
}

bool WaylandQtAMClientExtension::setWindowProperty(QWindow *window, const QString &name, const QVariant &value)
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
    if (int(qtam_extension::version()) != QWaylandClientExtension::version()) {
        qCWarning(LogWayland) << "Unsupported qtam_extension version:" << qtam_extension::version();
        return;
    }

    if (QWindow *window = m_windowToSurface.key(surface)) {
        const auto data = QByteArray::fromRawData(static_cast<const char *>(value->data), qsizetype(value->size));
        QDataStream ds(data);
        ds.setVersion(QDataStream::Qt_6_7);
        QVariant variantValue;
        ds >> variantValue;

        qCDebug(LogWayland) << "window property: client receive" << window << name << variantValue;
        setWindowPropertyHelper(window, name, variantValue);
    }
}

QT_END_NAMESPACE_AM

#include "moc_waylandqtamclientextension_p.cpp"
