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

#include "waylandqtamserverextension_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandResource>

#include <QtAppManCommon/logging.h>

QT_BEGIN_NAMESPACE_AM

WaylandQtAMServerExtension::WaylandQtAMServerExtension(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qtam_extension(compositor->display(), 1)
{ }

QVariantMap WaylandQtAMServerExtension::windowProperties(const QWaylandSurface *surface) const
{
    return m_windowProperties.value(surface);
}

void WaylandQtAMServerExtension::setWindowProperty(QWaylandSurface *surface, const QString &name, const QVariant &value)
{
    if (setWindowPropertyHelper(surface, name, value)) {
        QByteArray byteValue;
        QDataStream ds(&byteValue, QIODevice::WriteOnly);
        ds << value;

        Resource *target = resourceMap().value(surface->waylandClient());
        if (target) {
            qDebug(LogWaylandDebug) << "SERVER >>prop>>" << surface << name << value;
            send_window_property_changed(target->handle, surface->resource(), name, byteValue);
        }
    }
}

bool WaylandQtAMServerExtension::setWindowPropertyHelper(QWaylandSurface *surface, const QString &name, const QVariant &value)
{
    auto it = m_windowProperties.find(surface);
    if ((it == m_windowProperties.end()) || (it.value().value(name) != value)) {
        if (it == m_windowProperties.end())
            m_windowProperties[surface].insert(name, value);
        else
            it.value().insert(name, value);
        emit windowPropertyChanged(surface, name, value);
        return true;
    }
    return false;
}

void WaylandQtAMServerExtension::qtam_extension_set_window_property(QtWaylandServer::qtam_extension::Resource *resource, wl_resource *surface_resource, const QString &name, wl_array *value)
{
    Q_UNUSED(resource);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_resource);
    const QByteArray byteValue((const char *) value->data, value->size);
    QDataStream ds(byteValue);
    QVariant variantValue;
    ds >> variantValue;

    qCDebug(LogWaylandDebug) << "SERVER <<prop<<" << surface << name << variantValue;
    setWindowPropertyHelper(surface, name, variantValue);
}

QT_END_NAMESPACE_AM
