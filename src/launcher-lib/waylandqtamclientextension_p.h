/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QVariantMap>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include "private/qwayland-qtam-extension.h"

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

QT_FORWARD_DECLARE_CLASS(QWindow)

QT_BEGIN_NAMESPACE_AM

class WaylandQtAMClientExtension : public QWaylandClientExtensionTemplate<WaylandQtAMClientExtension>,
        public ::QtWayland::qtam_extension
{
    Q_OBJECT

public:
    WaylandQtAMClientExtension();
    ~WaylandQtAMClientExtension() override;

    QVariantMap windowProperties(QWindow *window) const;
    void setWindowProperty(QWindow *window, const QString &name, const QVariant &value);
    void clearWindowPropertyCache(QWindow *window);

signals:
    void windowPropertyChanged(QWindow *window, const QString &name, const QVariant &value);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool setWindowPropertyHelper(QWindow *window, const QString &name, const QVariant &value);
    void sendPropertyToServer(::wl_surface *surface, const QString &name, const QVariant &value);
    void qtam_extension_window_property_changed(wl_surface *surface, const QString &name, wl_array *value) override;

    QMap<QWindow *, QVariantMap> m_windowProperties;
    QMap<QWindow *, ::wl_surface *> m_windowToSurface;
};

QT_END_NAMESPACE_AM
