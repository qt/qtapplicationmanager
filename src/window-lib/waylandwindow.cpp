// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


#include "global.h"
#if QT_CONFIG(am_multi_process)
#include "logging.h"
#include "qml-utilities.h"
#include "applicationmanager.h"
#include "application.h"
#include "waylandwindow.h"
#include "waylandcompositor.h"
#include "waylandqtamserverextension_p.h"

#include <QWaylandWlShellSurface>

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

bool WaylandWindow::m_watchdogEnabled = true;

WaylandWindow::WaylandWindow(Application *app, WindowSurface *surf)
    : Window(app)
    , m_pingTimer(new QTimer(this))
    , m_pongTimer(new QTimer(this))
    , m_surface(surf)
{
    if (surf) {
        connect(surf, &WindowSurface::pong,
                this, &WaylandWindow::pongReceived);
        connect(m_surface, &QWaylandSurface::hasContentChanged, this, &WaylandWindow::onContentStateChanged);
        connect(m_surface, &QWaylandSurface::bufferSizeChanged, this, &Window::sizeChanged);

        m_pingTimer->setInterval(1000);
        m_pingTimer->setSingleShot(true);
        connect(m_pingTimer, &QTimer::timeout, this, &WaylandWindow::pingTimeout);
        m_pongTimer->setInterval(2000);
        m_pongTimer->setSingleShot(true);
        connect(m_pongTimer, &QTimer::timeout, this, &WaylandWindow::pongTimeout);

        connect(surf->compositor()->amExtension(), &WaylandQtAMServerExtension::windowPropertyChanged,
                this, [this](QWaylandSurface *surface, const QString &name, const QVariant &value) {
            if (surface == m_surface) {
                m_windowProperties[name] = value;
                emit windowPropertyChanged(name, value);
            }
        });

        connect(surf, &QWaylandSurface::surfaceDestroyed, this, [this]() {
            m_surface = nullptr;
            onContentStateChanged();
            emit waylandSurfaceChanged();
        });

        // Caching them so that Window::windowProperty and Window::windowProperties
        // still return the expected values even after the underlying wayland surface
        // is gone (content state NoSurface).
        m_windowProperties = m_surface->compositor()->amExtension()->windowProperties(m_surface);

        if (m_surface->isPopup()) {
            connect(m_surface, &WindowSurface::popupGeometryChanged,
                    this, &WaylandWindow::requestedPopupPositionChanged);
        }

        connect(m_surface, &WindowSurface::xdgSurfaceChanged,
                this, &WaylandWindow::waylandXdgSurfaceChanged);

        enableOrDisablePing();
    }
}

void WaylandWindow::pongReceived()
{
    m_pongTimer->stop();
    m_pingTimer->start();
}

void WaylandWindow::pongTimeout()
{
    if (!application())
        return;

    qCCritical(LogGraphics) << "Stopping application" << application()->id() << "because we did not receive a Wayland-Pong for" << m_pongTimer->interval() << "msec";
    ApplicationManager::instance()->stopApplicationInternal(application(), true);
}

void WaylandWindow::pingTimeout()
{
    m_pingTimer->stop();
    m_pongTimer->start();
    if (m_surface)
        m_surface->ping();
}

bool WaylandWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (m_surface) {
        const QVariant v = convertFromJSVariant(value);
        m_surface->compositor()->amExtension()->setWindowProperty(m_surface, name, v);
    }
    return (m_surface);
}

QVariant WaylandWindow::windowProperty(const QString &name) const
{
    return m_windowProperties.value(name);
}

QVariantMap WaylandWindow::windowProperties() const
{
    return m_windowProperties;
}

bool WaylandWindow::isPopup() const
{
    return m_surface ? m_surface->isPopup() : false;
}

QPoint WaylandWindow::requestedPopupPosition() const
{
    return m_surface ? m_surface->popupGeometry().topLeft() : QPoint();
}

Window::ContentState WaylandWindow::contentState() const
{
    if (m_surface)
        return m_surface->hasContent() ? SurfaceWithContent : SurfaceNoContent;
    else
        return NoSurface;
}

void WaylandWindow::enableOrDisablePing()
{
    if (m_watchdogEnabled) {
        m_pingTimer->stop();
        m_pongTimer->stop();

        if (m_surface && m_surface->hasContent())
            pingTimeout();
    }
}

void WaylandWindow::onContentStateChanged()
{
    qCDebug(LogGraphics) << this << "of" << applicationId() << "contentState changed to" << contentState();

    enableOrDisablePing();
    emit contentStateChanged();
}

QString WaylandWindow::applicationId() const
{
    if (application())
        return application()->id();
    else if (m_surface && m_surface->client())
        return u"[pid: %1]"_s.arg(m_surface->client()->processId());
    else
        return u"[external app]"_s;
}

QSize WaylandWindow::size() const
{
    return m_surface->bufferSize();
}

void WaylandWindow::resize(const QSize &newSize)
{
    if (!m_surface)
        return;

    qCDebug(LogGraphics) << this << "of" << applicationId() << "sending resize request for surface"
                         << m_surface << "to" << newSize;

    m_surface->sendResizing(newSize);
}

QWaylandQuickSurface* WaylandWindow::waylandSurface() const
{
    return m_surface;
}

QWaylandXdgSurface *WaylandWindow::waylandXdgSurface() const
{
    return m_surface ? m_surface->xdgSurface() : nullptr;
}

void WaylandWindow::close()
{
    if (m_surface)
        m_surface->close();
}

QT_END_NAMESPACE_AM

#endif // QT_CONFIG(am_multi_process)

#include "moc_waylandwindow.cpp"
