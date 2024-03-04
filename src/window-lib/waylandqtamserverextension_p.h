// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WAYLANDQTAMSERVEREXTENSION_P_H
#define WAYLANDQTAMSERVEREXTENSION_P_H

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

#endif // WAYLANDQTAMSERVEREXTENSION_P_H
