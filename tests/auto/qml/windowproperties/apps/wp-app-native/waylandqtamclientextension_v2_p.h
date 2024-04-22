// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WAYLANDQTAMCLIENTEXTENSION_V2_P_H
#define WAYLANDQTAMCLIENTEXTENSION_V2_P_H

#include <QVariantMap>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include "qwayland-qtam-extension.h"

QT_FORWARD_DECLARE_CLASS(QWindow)

// This is a copy of the built-in class WaylandQtAMClientExtension (version 1). Not all
// functionality is actually used for the test, but it makes it easier to keep the code in sync
// this way, as they share ~90% of the implementation.

class WaylandQtAMClientExtensionV2 : public QWaylandClientExtensionTemplate<WaylandQtAMClientExtensionV2>,
        public ::QtWayland::qtam_extension
{
    Q_OBJECT

public:
    WaylandQtAMClientExtensionV2();
    ~WaylandQtAMClientExtensionV2() override;

    QVariantMap windowProperties(QWindow *window) const;
    bool setWindowProperty(QWindow *window, const QString &name, const QVariant &value);
    void clearWindowPropertyCache(QWindow *window);

Q_SIGNALS:
    void windowPropertyChanged(QWindow *window, const QString &name, const QVariant &value);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool setWindowPropertyHelper(QWindow *window, const QString &name, const QVariant &value);
    void sendPropertyToServer(::wl_surface *surface, const QString &name, const QVariant &value);
    void qtam_extension_window_property_changed(wl_surface *surface, const QString &name, wl_array *value) override;

    QMap<QWindow *, QVariantMap> m_windowProperties;    // AXIVION Line Qt-QMapWithPointerKey: cleared on destroyed signal
    QMap<QWindow *, ::wl_surface *> m_windowToSurface;  // AXIVION Line Qt-QMapWithPointerKey: cleared on destroyed signal
};

#endif // WAYLANDQTAMCLIENTEXTENSION_V2_P_H
