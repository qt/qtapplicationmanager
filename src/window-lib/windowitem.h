// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QQuickItem>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Window;
class InProcessWindow;
#if defined(AM_MULTI_PROCESS)
class WaylandWindow;
class WaylandQuickIgnoreKeyItem;
#endif // AM_MULTI_PROCESS


class WindowItem : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/WindowItem 2.0")

    Q_PROPERTY(Window* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool primary READ primary NOTIFY primaryChanged)
    Q_PROPERTY(bool objectFollowsItemSize READ objectFollowsItemSize
                                          WRITE setObjectFollowsItemSize
                                          NOTIFY objectFollowsItemSizeChanged)

    Q_PROPERTY(QQmlListProperty<QObject> contentItemData READ contentItemData NOTIFY contentItemDataChanged)
    Q_CLASSINFO("DefaultProperty", "contentItemData")

public:
    WindowItem(QQuickItem *parent = nullptr);
    ~WindowItem();

    Window *window() const;
    void setWindow(Window *window);

    bool primary() const;
    Q_INVOKABLE void makePrimary();

    bool objectFollowsItemSize() { return m_objectFollowsItemSize; }
    void setObjectFollowsItemSize(bool value);

    QQmlListProperty<QObject> contentItemData();

    static void contentItemData_append(QQmlListProperty<QObject> *property, QObject *value);
    static qsizetype contentItemData_count(QQmlListProperty<QObject> *property);
    static QObject *contentItemData_at(QQmlListProperty<QObject> *property, qsizetype index);
    static void contentItemData_clear(QQmlListProperty<QObject> *property);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void windowChanged();
    void primaryChanged();
    void objectFollowsItemSizeChanged();
    void contentItemDataChanged();

private slots:
    void updateImplicitSize();
private:
    void createImpl(bool inProcess);

    struct Impl {
        Impl(WindowItem *windowItem) : q(windowItem) {}
        virtual ~Impl() {}
        virtual void setup(Window *window) = 0;
        virtual void tearDown() = 0;
        virtual void updateSize(const QSizeF &newSize) = 0;
        virtual bool isInProcess() const = 0;
        virtual Window *window() const = 0;
        virtual void setupPrimaryView() = 0;
        virtual void setupSecondaryView() = 0;
        virtual void forwardActiveFocus() = 0;
        WindowItem *q;
    };

    struct InProcessImpl : public Impl {
        InProcessImpl(WindowItem *windowItem) : Impl(windowItem) {}
        void setup(Window *window) override;
        void tearDown() override;
        void updateSize(const QSizeF &newSize) override;
        bool isInProcess() const override { return true; }
        Window *window() const override;
        void setupPrimaryView() override;
        void setupSecondaryView() override;
        void forwardActiveFocus() override;

        InProcessWindow *m_inProcessWindow{nullptr};
        QQuickItem *m_shaderEffectSource{nullptr};
    };

#if defined(AM_MULTI_PROCESS)
    struct WaylandImpl : public Impl {
        WaylandImpl(WindowItem *windowItem) : Impl(windowItem) {}
        ~WaylandImpl();
        void setup(Window *window) override;
        void tearDown() override;
        void updateSize(const QSizeF &newSize) override;
        bool isInProcess() const override { return false; }
        Window *window() const override;
        void setupPrimaryView() override;
        void setupSecondaryView() override;
        void createWaylandItem();
        void forwardActiveFocus() override;

        WaylandWindow *m_waylandWindow{nullptr};
        WaylandQuickIgnoreKeyItem *m_waylandItem{nullptr};
    };
#endif // AM_MULTI_PROCESS

    Impl *m_impl{nullptr};
    bool m_objectFollowsItemSize{true};

    // The only purpose of this item is to ensure all WindowItem children added by System UI code ends up
    // in front of WindowItem's internal QWaylandQuickItem (or the application's root item in case of
    // inprocess)
    QQuickItem *m_contentItem;
};

QT_END_NAMESPACE_AM
