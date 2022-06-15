// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <QtAppManCommon/global.h>
#include <QtAppManManager/application.h>

QT_BEGIN_NAMESPACE_AM

class WindowItem;

// A Window object exists for every application window that is managed by the application manager
// and has been shown (mapped) at least once.
// In the Wayland case, every Window is a surface, but not every surface is a Window. That's why
// there is a separate WindowSurface class in waylandcompositor.h.

class Window : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/WindowObject 2.0 UNCREATABLE")

    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(ContentState contentState READ contentState NOTIFY contentStateChanged)
    Q_PROPERTY(Application* application READ application CONSTANT)
    Q_PROPERTY(bool popup READ isPopup CONSTANT)
    Q_PROPERTY(QPoint requestedPopupPosition READ requestedPopupPosition NOTIFY requestedPopupPositionChanged)

public:

    enum ContentState {
        SurfaceWithContent,
        SurfaceNoContent,
        NoSurface
    };
    Q_ENUM(ContentState)

    Window(Application *app);
    virtual ~Window();

    virtual bool isInProcess() const = 0;
    virtual Application *application() const;

    // Controls how many items (which are views from a model-view perspective) are currently rendering this window
    void registerItem(WindowItem *item);
    void unregisterItem(WindowItem *item);
    const QSet<WindowItem*> &items() const { return m_items; }

    void setPrimaryItem(WindowItem *item);
    WindowItem *primaryItem() const { return m_primaryItem; }

    // This really depends on the windowing system - currently only Wayland with xdg-shell 6
    // supports popups natively
    virtual bool isPopup() const = 0;
    virtual QPoint requestedPopupPosition() const = 0;

    // Whether there's any view (WindowItem) holding a reference to this window
    bool isBeingDisplayed() const;

    virtual ContentState contentState() const = 0;

    Q_INVOKABLE virtual bool setWindowProperty(const QString &name, const QVariant &value) = 0;
    Q_INVOKABLE virtual QVariant windowProperty(const QString &name) const = 0;
    Q_INVOKABLE virtual QVariantMap windowProperties() const = 0;

    Q_INVOKABLE virtual void close() = 0;

    virtual QSize size() const = 0;
    Q_INVOKABLE virtual void resize(const QSize &size) = 0;

signals:
    void sizeChanged();
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void isBeingDisplayedChanged();
    void contentStateChanged();
    void requestedPopupPositionChanged();

protected:
    QPointer<Application> m_application;

    QSet<WindowItem*> m_items;
    WindowItem *m_primaryItem{nullptr};
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(Window*))
