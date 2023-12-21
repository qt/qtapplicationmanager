// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "inprocesswindow.h"
#include "inprocesssurfaceitem.h"

QT_BEGIN_NAMESPACE_AM

InProcessWindow::InProcessWindow(Application *app, const QSharedPointer<InProcessSurfaceItem> &surfaceItem)
    : Window(app)
    , m_surfaceItem(surfaceItem)
{
    connect(m_surfaceItem.data(), &InProcessSurfaceItem::windowPropertyChanged,
            this, &Window::windowPropertyChanged);

    connect(m_surfaceItem.data(), &QQuickItem::widthChanged, this, &Window::sizeChanged);
    connect(m_surfaceItem.data(), &QQuickItem::heightChanged, this, &Window::sizeChanged);

    // queued to emulate the behavior of the out-of-process counterpart
    connect(m_surfaceItem.data(), &InProcessSurfaceItem::visibleClientSideChanged,
            this, &InProcessWindow::onVisibleClientSideChanged,
            Qt::QueuedConnection);
}

InProcessWindow::~InProcessWindow()
{
    emit m_surfaceItem->released();
}

bool InProcessWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_surfaceItem->setWindowProperty(name, value);
}

QVariant InProcessWindow::windowProperty(const QString &name) const
{
    return m_surfaceItem->windowProperty(name);
}

QVariantMap InProcessWindow::windowProperties() const
{
    return m_surfaceItem->windowPropertiesAsVariantMap();
}

void InProcessWindow::onVisibleClientSideChanged()
{
    if (!m_surfaceItem->visibleClientSide()) {
        setContentState(Window::SurfaceNoContent);
        setContentState(Window::NoSurface);
    }
}

void InProcessWindow::setContentState(ContentState newState)
{
    if (m_contentState != newState) {
        m_contentState = newState;
        emit contentStateChanged();
    }
}

QSize InProcessWindow::size() const
{
    return m_surfaceItem->size().toSize();
}

void InProcessWindow::resize(const QSize &size)
{
    m_surfaceItem->setSize(size);
}

void InProcessWindow::close()
{
    m_surfaceItem->close();
}

bool InProcessWindow::isPopup() const
{
    return false;
}

QPoint InProcessWindow::requestedPopupPosition() const
{
    return QPoint();
}

QT_END_NAMESPACE_AM

#include "moc_inprocesswindow.cpp"
