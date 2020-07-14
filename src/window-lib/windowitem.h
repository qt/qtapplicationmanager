/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#pragma once

#include <QQuickItem>
#include <QtAppManCommon/global.h>

#if defined(AM_MULTI_PROCESS)
#include <QWaylandQuickItem>
#endif // AM_MULTI_PROCESS

QT_BEGIN_NAMESPACE_AM

class Window;
class InProcessWindow;
#if defined(AM_MULTI_PROCESS)
class WaylandWindow;
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

    Q_PROPERTY(QQmlListProperty<QObject> contentItemData READ contentItemData)
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
    static int contentItemData_count(QQmlListProperty<QObject> *property);
    static QObject *contentItemData_at(QQmlListProperty<QObject> *property, int index);
    static void contentItemData_clear(QQmlListProperty<QObject> *property);

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void windowChanged();
    void primaryChanged();
    void objectFollowsItemSizeChanged();
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

        WaylandWindow *m_waylandWindow{nullptr};
        QWaylandQuickItem *m_waylandItem{nullptr};
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
