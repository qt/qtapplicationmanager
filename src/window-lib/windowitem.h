// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WINDOWITEM_H
#define WINDOWITEM_H

#if 0
#pragma qt_sync_skip_header_check
#endif

#include <QtQuick/QQuickItem>
#include <QtQuick/private/qquickfocusscope_p.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Window;
class InProcessWindow;
#if QT_CONFIG(am_multi_process)
class WaylandWindow;
class WaylandQuickIgnoreKeyItem;
#endif


class WindowItem : public QQuickFocusScope
{
    Q_OBJECT

    //TODO for Qt7: rename the "window" property, as QQuickItem has a window() getter and a
    // windowChanged() signal.
    Q_PROPERTY(QtAM::Window *window READ window WRITE setWindow NOTIFY windowChanged FINAL)
    Q_PROPERTY(bool primary READ primary NOTIFY primaryChanged FINAL)
    Q_PROPERTY(bool objectFollowsItemSize READ objectFollowsItemSize
                                          WRITE setObjectFollowsItemSize
                                          NOTIFY objectFollowsItemSizeChanged)

    Q_PROPERTY(QQmlListProperty<QObject> contentItemData READ contentItemData NOTIFY contentItemDataChanged FINAL)
    Q_PROPERTY(bool focusOnClick READ focusOnClick WRITE setFocusOnClick NOTIFY focusOnClickChanged REVISION(2, 7) FINAL)
    Q_PROPERTY(QQuickItem *backingItem READ backingItem NOTIFY windowChanged REVISION(2, 7) FINAL)
    Q_CLASSINFO("DefaultProperty", "contentItemData")

public:
    WindowItem(QQuickItem *parent = nullptr);
    ~WindowItem() override;

    Window *window() const;
    void setWindow(Window *window);
    QQuickItem *backingItem() const;

    bool primary() const;
    Q_INVOKABLE void makePrimary();

    bool objectFollowsItemSize() { return m_objectFollowsItemSize; }
    void setObjectFollowsItemSize(bool value);

    QQmlListProperty<QObject> contentItemData();

    static void contentItemData_append(QQmlListProperty<QObject> *property, QObject *value);
    static qsizetype contentItemData_count(QQmlListProperty<QObject> *property);
    static QObject *contentItemData_at(QQmlListProperty<QObject> *property, qsizetype index);
    static void contentItemData_clear(QQmlListProperty<QObject> *property);

    bool focusOnClick() const;
    void setFocusOnClick(bool newFocusOnClick);
    Q_SIGNAL void focusOnClickChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

Q_SIGNALS:
    void windowChanged(); // clazy:exclude=overloaded-signal
    void primaryChanged();
    void objectFollowsItemSizeChanged();
    void contentItemDataChanged();

private Q_SLOTS:
    void updateImplicitSize();
private:
    void createImpl(bool inProcess);

    struct Impl
    {
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
        virtual bool focusOnClick() const = 0;
        virtual void setFocusOnClick(bool focusOnClick) = 0;
        virtual QQuickItem *backingItem() = 0;
        WindowItem *q;

        Q_DISABLE_COPY_MOVE(Impl)
    };

    struct InProcessImpl : public Impl
    {
        InProcessImpl(WindowItem *windowItem) : Impl(windowItem) {}
        void setup(Window *window) override;
        void tearDown() override;
        void updateSize(const QSizeF &newSize) override;
        bool isInProcess() const override { return true; }
        Window *window() const override;
        void setupPrimaryView() override;
        void setupSecondaryView() override;
        void forwardActiveFocus() override;
        bool focusOnClick() const override;
        void setFocusOnClick(bool focusOnClick) override;
        QQuickItem *backingItem() override;

        InProcessWindow *m_inProcessWindow{nullptr};
        QQuickItem *m_shaderEffectSource{nullptr};
    };

#if QT_CONFIG(am_multi_process)
    struct WaylandImpl : public Impl
    {
        WaylandImpl(WindowItem *windowItem) : Impl(windowItem) {}
        ~WaylandImpl() override;
        void setup(Window *window) override;
        void tearDown() override;
        void updateSize(const QSizeF &newSize) override;
        bool isInProcess() const override { return false; }
        Window *window() const override;
        void setupPrimaryView() override;
        void setupSecondaryView() override;
        void createWaylandItem();
        void forwardActiveFocus() override;
        bool focusOnClick() const override;
        void setFocusOnClick(bool focusOnClick) override;
        QQuickItem *backingItem() override;

        WaylandWindow *m_waylandWindow{nullptr};
        WaylandQuickIgnoreKeyItem *m_waylandItem{nullptr};
    };
#endif // QT_CONFIG(am_multi_process)

    Impl *m_impl{nullptr};
    bool m_objectFollowsItemSize{true};

    // The only purpose of this item is to ensure all WindowItem children added by System UI code ends up
    // in front of WindowItem's internal QWaylandQuickItem (or the application's root item in case of
    // inprocess)
    QQuickItem *m_contentItem;
};

QT_END_NAMESPACE_AM

#endif // WINDOWITEM_H
