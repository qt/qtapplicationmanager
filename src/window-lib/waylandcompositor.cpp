// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QQuickView>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandKeymap>

#include <wayland-server.h>

#include "global.h"
#include "logging.h"
#include "windowmanager.h"
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

bool WaylandCompositor::s_taggedLogging = false;

QObject *WaylandCompositor::preConstructor(QObject *parent)
{
    static bool once = false;
    if (!once) {
        const auto waylandDebug = qEnvironmentVariable("WAYLAND_DEBUG");
        const bool amTaggedWaylandDebug = (qEnvironmentVariableIntValue("AM_TAGGED_WAYLAND_DEBUG") == 1);

        const bool waylandDebugBoth = (waylandDebug == u"1");
        const bool waylandDebugServer = (waylandDebug == u"server");
        const bool waylandDebugClient = (waylandDebug == u"client");
        const bool nestedLogging = waylandDebugBoth && (qApp->platformName().startsWith(u"wayland"));

        if (amTaggedWaylandDebug) {
            if (waylandDebugServer || waylandDebugBoth) {
                qCInfo(LogGraphics) << "AM_TAGGED_WAYLAND_DEBUG is set. All server side WAYLAND_DEBUG logs are now tagged with "
                                      "an <app-id> to make them distinguishable (see documentation).";

                s_taggedLogging = true;

                // We need to unset/change WAYLAND_DEBUG, so that libwayland-server does not
                // activate its own logger in additon to our own.
                if (waylandDebugBoth) {
                    if (nestedLogging) {
                        qCInfo(LogGraphics) << "The application manager is running as a nested compositor and WAYLAND_DEBUG=1 is set. "
                                              "WAYLAND_DEBUG will be unset for applications to avoid a 3-way mixed client/server log.";
                        qunsetenv("WAYLAND_DEBUG");
                    } else {
                        qputenv("WAYLAND_DEBUG", "client");
                    }
                } else {
                    qunsetenv("WAYLAND_DEBUG");
                }
            } else if (waylandDebugClient) {
                qCInfo(LogGraphics) << "AM_TAGGED_WAYLAND_DEBUG is set, but this does not affect WAYLAND_DEBUG=client.";
            } else {
                qCInfo(LogGraphics) << "AM_TAGGED_WAYLAND_DEBUG is set but ignored, because WAYLAND_DEBUG is not set.";
            }
        } else if (nestedLogging) {
            // we are a nested compositor and logging with WAYLAND_DEBUG=1 just creates a mess,
            // as both the server and client side output is intermixed without any tagging.
            qCInfo(LogGraphics) << "The application manager is running as a nested compositor and WAYLAND_DEBUG=1 is set. "
                                  "This will produce a mixed client/server log that is not readable. "
                                  "You should also set AM_TAGGED_WAYLAND_DEBUG=1 (see documentation).";
        }
        once = true;
    }
    return parent;
}

WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName)
    : QWaylandQuickCompositor(preConstructor(nullptr))
    , m_wlShell(new QWaylandWlShell(this))
    , m_xdgShell(new QWaylandXdgShell(this))
    , m_amExtension(new WaylandQtAMServerExtension(this))
    , m_qtTextInputMethodManager(new QWaylandQtTextInputMethodManager(this))
    , m_textInputManager(new QWaylandTextInputManager(this))
{
    setupLogging();

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

void WaylandCompositor::setupLogging()
{
    // only need custom logging, if we need to tag
    if (!s_taggedLogging)
        return;

    // The actual logging code below is mostly a verbatim copy of the code in libwayland-server
    // in order to keep the log format consistent.
    wl_display_add_protocol_logger(display(), [](void *user_data,
                                                 enum wl_protocol_logger_type direction,
                                                 const struct wl_protocol_logger_message *message) {
        Q_ASSERT(message->resource);
        if (!message->resource)
            return;
        bool send = (direction == WL_PROTOCOL_LOGGER_EVENT);

        char *buffer;
        size_t buffer_length;
        auto f = ::open_memstream(&buffer, &buffer_length);
        if (!f)
            return;

        ::timespec tp;
        ::clock_gettime(CLOCK_REALTIME, &tp);
        // Wayland bug -- this will overflow unsigned int, but we need to stay compatible with
        // the timestamps generated by the libwayland-client logger.
        unsigned int time = (tp.tv_sec * 1000000L) + (tp.tv_nsec / 1000);

        QByteArray clientId;
        if (auto *client = QWaylandClient::fromWlClient(static_cast<WaylandCompositor *>(user_data), message->resource->client)) {
            const auto apps = ApplicationManager::instance()->fromProcessId(client->processId());
            for (const auto *app : apps) {
                if (!clientId.isEmpty())
                    clientId += '/';
                clientId += app->id().toUtf8();
            }
            if (clientId.isEmpty())
                clientId = "pid:" + QByteArray::number(client->processId());
        } else {
            clientId = "client:0x" + QByteArray::number(reinterpret_cast<std::uintptr_t>(message->resource->client), 16);
        }

        ::fprintf(f, "[%7u.%03u] <%s> %s%s#%u.%s(",
                  time / 1000, time % 1000,
                  clientId.constData(),
                  send ? " -> " : "",
                  message->resource->object.interface->name, message->resource->object.id,
                  message->message->name);

        const char *signature = message->message->signature;
        char *realSignatureStart = nullptr;
        auto version = ::strtoul(signature, &realSignatureStart, 10);
        if (version && realSignatureStart)
            signature = realSignatureStart;

        for (int i = 0; i < message->arguments_count; ++i, ++signature) {
            if (i > 0)
                fprintf(f, ", ");

            while (signature && (*signature == '?'))
                ++signature; // ignore nullable

            switch (signature ? *signature : '\0') {
            case 'u':
                fprintf(f, "%u", message->arguments[i].u);
                break;
            case 'i':
                fprintf(f, "%d", message->arguments[i].i);
                break;
            case 'f':
                // The magic number 390625 is 1e8 / 256
                if (message->arguments[i].f >= 0) {
                    fprintf(f, "%d.%08d",
                            message->arguments[i].f / 256,
                            390625 * (message->arguments[i].f % 256));
                } else {
                    fprintf(f, "-%d.%08d",
                            message->arguments[i].f / -256,
                            -390625 * (message->arguments[i].f % 256));
                }
                break;
            case 's':
                if (message->arguments[i].s)
                    fprintf(f, "\"%s\"", message->arguments[i].s);
                else
                    fprintf(f, "nil");
                break;
            case 'o':
                if (message->arguments[i].o)
                    fprintf(f, "%s#%u",
                            message->arguments[i].o->interface->name,
                            message->arguments[i].o->id);
                else
                    fprintf(f, "nil");
                break;
            case 'n': {
                uint32_t nval;
                nval = message->arguments[i].n;

                fprintf(f, "new id %s#",
                        (message->message->types[i]) ?
                            message->message->types[i]->name :
                            "[unknown]");
                if (nval != 0)
                    fprintf(f, "%u", nval);
                else
                    fprintf(f, "nil");
                break;
            }
            case 'a':
                fprintf(f, "array[%zu]", message->arguments[i].a->size);
                break;
            case 'h':
                fprintf(f, "fd %d", message->arguments[i].h);
                break;
            default:
                qWarning() << "Unknown Wayland signature byte:" << (signature ? *signature : '\0');
                Q_ASSERT(false);
                break;
            }

        }
        ::fprintf(f, ")\n");

        if (::fclose(f) == 0) {
            ::fprintf(stderr, "%s", buffer);
            ::free(buffer);
        }
    }, this);
}

void WaylandCompositor::registerOutputWindow(QQuickWindow* window)
{
    auto output = new QWaylandQuickOutput(this, window);
    output->setSizeFollowsWindow(true);
    m_outputs.append(output);
    window->winId();
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
