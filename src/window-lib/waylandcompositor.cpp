// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQuickView>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandKeymap>

#include "global.h"
#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "waylandcompositor.h"

#include <QWaylandWlShell>
#include <QWaylandXdgShell>
#include <QWaylandQuickOutput>
#include <QWaylandTextInputManager>
#include <QWaylandQtTextInputMethodManager>
#include <QWaylandQtWindowManager>
#include "waylandqtamserverextension_p.h"

QT_BEGIN_NAMESPACE_AM

WindowSurface::WindowSurface(WaylandCompositor *comp, QWaylandClient *client, uint id, int version)
    : QWaylandQuickSurface(comp, client, id, version)
    , m_surface(this)
    , m_compositor(comp)
{ }

void WindowSurface::setShellSurface(QWaylandWlShellSurface *shellSurface)
{
    Q_ASSERT(!m_xdgSurface);
    m_wlSurface = shellSurface;
    connect(m_wlSurface, &QWaylandWlShellSurface::pong,
            this, &WindowSurface::pong);
}

void WindowSurface::sendResizing(const QSize &size)
{
    if (m_topLevel)
        m_topLevel->sendResizing(size);
    else if (m_popup)
        ; // do nothing
    else
        m_wlSurface->sendConfigure(size, QWaylandWlShellSurface::NoneEdge);
}

QWaylandXdgSurface *WindowSurface::xdgSurface() const
{
    return m_xdgSurface;
}

WaylandCompositor *WindowSurface::compositor() const
{
    return m_compositor;
}

QString WindowSurface::applicationId() const
{
    if (m_xdgSurface && m_xdgSurface->toplevel())
        return m_xdgSurface->toplevel()->appId();
    return { };
}

bool WindowSurface::isPopup() const
{
    return m_popup;
}

QRect WindowSurface::popupGeometry() const
{
    return m_popup ? m_popup->configuredGeometry() : QRect();
}

QWaylandSurface *WindowSurface::surface() const
{
    return m_surface;
}

qint64 WindowSurface::processId() const
{
    return m_surface->client()->processId();
}

QWindow *WindowSurface::outputWindow() const
{
    if (QWaylandView *v = m_surface->primaryView())
        return v->output()->window();
    return nullptr;
}

void WindowSurface::ping()
{
    if (m_xdgSurface) {
        // FIXME: ping should be done per-application, not per-window
        m_compositor->xdgPing(this);
    } else {
        m_wlSurface->ping();
    }
}

void WindowSurface::close()
{
    if (m_topLevel) {
        m_topLevel->sendClose();
    } else if (m_popup) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QWaylandXdgPopupPrivate::get(m_popup)->send_popup_done();
#else
        m_popup->sendPopupDone();
#endif
    } else {
        qCWarning(LogGraphics) << "The Wayland surface" << this << "is not using the XDG Shell extension. Unable to send close signal.";
    }
}

WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName)
    : QWaylandQuickCompositor()
    , m_wlShell(new QWaylandWlShell(this))
    , m_xdgShell(new QWaylandXdgShell(this))
    , m_amExtension(new WaylandQtAMServerExtension(this))
    , m_qtTextInputMethodManager(new QWaylandQtTextInputMethodManager(this))
    , m_textInputManager(new QWaylandTextInputManager(this))
{
    // We are instantiating both the semi-official TextInputManager protocol (which has some
    // traction upstream, but also has known defects) and our own QtTextInputMethodManager
    // (which was added in Qt 6 to mimic our internal C++ interfaces and works perfectly stable).
    // Clients can then choose which protocol they want to use. QtVirtualKeyboard will use the
    // QtTextInputMethodManager automatically.
    //TODO: find out, why all 6.4 based clients are crashing at startup, if we instantiate the two
    //      extensions in the opposite order.

    m_wlShell->setParent(this);
    m_xdgShell->setParent(this);
    m_amExtension->setParent(this);
    m_textInputManager->setParent(this);
    m_qtTextInputMethodManager->setParent(this);

    setSocketName(waylandSocketName.toUtf8());
    registerOutputWindow(window);

    connect(this, &QWaylandCompositor::surfaceRequested, this, &WaylandCompositor::doCreateSurface);

    connect(m_wlShell, &QWaylandWlShell::wlShellSurfaceRequested, this, &WaylandCompositor::createWlSurface);

    connect(m_xdgShell, &QWaylandXdgShell::xdgSurfaceCreated, this, &WaylandCompositor::onXdgSurfaceCreated);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated, this, &WaylandCompositor::onTopLevelCreated);
    connect(m_xdgShell, &QWaylandXdgShell::popupCreated, this, &WaylandCompositor::onPopupCreated);
    connect(m_xdgShell, &QWaylandXdgShell::pong, this, &WaylandCompositor::onXdgPongReceived);

    auto wmext = new QWaylandQtWindowManager(this);
    wmext->setParent(this);
    connect(wmext, &QWaylandQtWindowManager::openUrl, this, [](QWaylandClient *client, const QUrl &url) {
        if (!ApplicationManager::instance()->fromProcessId(client->processId()).isEmpty())
            ApplicationManager::instance()->openUrl(url.toString());
    });

    create();

    // set a sensible default keymap
    defaultSeat()->keymap()->setLayout(QLocale::system().name().section(qL1C('_'), 1, 1).toLower());
}

WaylandCompositor::~WaylandCompositor()
{
    // QWayland leaks like sieve everywhere, but we need this explicit delete to be able
    // to suppress the rest via LSAN leak suppression files
    delete defaultSeat();
}

void WaylandCompositor::xdgPing(WindowSurface* surface)
{
    uint serial = m_xdgShell->ping(surface->client());
    m_xdgPingMap[serial] = surface;
}

void WaylandCompositor::onXdgPongReceived(uint serial)
{
    auto surface = m_xdgPingMap.take(serial);
    if (surface) {
        emit surface->pong();
    }
}

void WaylandCompositor::registerOutputWindow(QQuickWindow* window)
{
    auto output = new QWaylandQuickOutput(this, window);
    output->setSizeFollowsWindow(true);
    m_outputs.append(output);
    window->winId();

    // Qt doesn't automatically de-focus a Wayland client, if a control in the Sys-UI gets focus
    //TODO: check if upstreaming to QtWaylandCompositor is an option
    connect(window, &QQuickWindow::activeFocusItemChanged,
            this, [this, window]() {
        QQuickItem *lastFocusItem = window->property("_am_lastFocusItem").value<QQuickItem *>();
        QQuickItem *currentFocusItem = window->activeFocusItem();

        window->setProperty("_am_lastFocusItem", QVariant::fromValue(currentFocusItem));

        if ((lastFocusItem != currentFocusItem)
            && qobject_cast<QWaylandQuickItem *>(lastFocusItem)
            && !qobject_cast<QWaylandQuickItem *>(currentFocusItem)) {

            if (QWaylandSeat *target = defaultSeat())
                target->setKeyboardFocus(nullptr);
        }
    });
}

WaylandQtAMServerExtension *WaylandCompositor::amExtension()
{
    return m_amExtension;
}

void WaylandCompositor::doCreateSurface(QWaylandClient *client, uint id, int version)
{
    (void) new WindowSurface(this, client, id, version);
}


void WaylandCompositor::createWlSurface(QWaylandSurface *surface, const QWaylandResource &resource)
{
    WindowSurface *windowSurface = static_cast<WindowSurface *>(surface);
    QWaylandWlShellSurface *ss = new QWaylandWlShellSurface(m_wlShell, windowSurface, resource);
    windowSurface->setShellSurface(ss);

    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
            emit this->surfaceMapped(static_cast<WindowSurface*>(windowSurface));
    });
}


void WaylandCompositor::onXdgSurfaceCreated(QWaylandXdgSurface *xdgSurface)
{
    WindowSurface *windowSurface = static_cast<WindowSurface*>(xdgSurface->surface());

    Q_ASSERT(!windowSurface->m_wlSurface);
    windowSurface->m_xdgSurface = xdgSurface;

    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
            emit this->surfaceMapped(windowSurface);
    });
    emit windowSurface->xdgSurfaceChanged();
}

void WaylandCompositor::onTopLevelCreated(QWaylandXdgToplevel *topLevel, QWaylandXdgSurface *xdgSurface)
{
    WindowSurface *windowSurface = static_cast<WindowSurface*>(xdgSurface->surface());

    Q_ASSERT(!windowSurface->m_wlSurface);
    Q_ASSERT(windowSurface->m_xdgSurface == xdgSurface);

    windowSurface->m_topLevel = topLevel;
}

void WaylandCompositor::onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface)
{
    WindowSurface *windowSurface = static_cast<WindowSurface*>(xdgSurface->surface());

    Q_ASSERT(!windowSurface->m_wlSurface);
    Q_ASSERT(windowSurface->m_xdgSurface == xdgSurface);

    windowSurface->m_popup = popup;

    connect(popup, &QWaylandXdgPopup::configuredGeometryChanged,
            windowSurface, &WindowSurface::popupGeometryChanged);
}

QT_END_NAMESPACE_AM

#include "moc_waylandcompositor.cpp"
