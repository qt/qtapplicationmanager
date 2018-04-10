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

#include "inprocesssurfaceitem.h"
#include "fakeapplicationmanagerwindow.h"

QT_BEGIN_NAMESPACE_AM

InProcessSurfaceItem::InProcessSurfaceItem(FakeApplicationManagerWindow *content)
    : m_contentItem(content)
{
    content->m_surfaceItem = this;
    m_windowProperties = content->m_windowProperties;
    setParentItem(content->parentItem());
    content->setParentItem(this);
    setWidth(content->width());
    setHeight(content->height());
}

InProcessSurfaceItem::~InProcessSurfaceItem()
{
    if (m_contentItem)
        m_contentItem->m_surfaceItem = nullptr;
}

QSharedPointer<QObject> InProcessSurfaceItem::windowProperties()
{
    return m_windowProperties;
}

void InProcessSurfaceItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (m_contentItem) {
        m_contentItem->setWidth(newGeometry.width());
        m_contentItem->setHeight(newGeometry.height());
    }
}

QT_END_NAMESPACE_AM
