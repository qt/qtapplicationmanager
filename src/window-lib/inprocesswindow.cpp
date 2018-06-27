/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include "inprocesswindow.h"
#include "inprocesssurfaceitem.h"

QT_BEGIN_NAMESPACE_AM

InProcessWindow::InProcessWindow(AbstractApplication *app, const QSharedPointer<InProcessSurfaceItem> &surfaceItem)
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
    return QSize(m_surfaceItem->width(), m_surfaceItem->height());
}

void InProcessWindow::close()
{
    m_surfaceItem->close();
}

QT_END_NAMESPACE_AM
