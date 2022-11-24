/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QtWaylandCompositor/QWaylandCompositorExtensionTemplate>
#include <QtCore/QVariant>
#include "private/qwayland-server-qtam-extension.h"

#include <QtAppManCommon/global.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_FORWARD_DECLARE_CLASS(QWaylandCompositor)
QT_FORWARD_DECLARE_CLASS(QWaylandSurface)

QT_BEGIN_NAMESPACE_AM

class WaylandQtAMServerExtension : public QWaylandCompositorExtensionTemplate<WaylandQtAMServerExtension>,
        public ::QtWaylandServer::qtam_extension
{
    Q_OBJECT

public:
    WaylandQtAMServerExtension(QWaylandCompositor *compositor);

    QVariantMap windowProperties(const QWaylandSurface *surface) const;
    void setWindowProperty(QWaylandSurface *surface, const QString &name, const QVariant &value);

signals:
    void windowPropertyChanged(QWaylandSurface *surface, const QString &name, const QVariant &value);

private:
    bool setWindowPropertyHelper(QWaylandSurface *surface, const QString &name, const QVariant &value);
    void qtam_extension_set_window_property(Resource *resource, wl_resource *surface_resource, const QString &name, wl_array *value) override;

    QMap<const QWaylandSurface *, QVariantMap> m_windowProperties;
};

QT_END_NAMESPACE_AM
