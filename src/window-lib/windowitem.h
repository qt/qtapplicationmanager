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

    Q_PROPERTY(Window* window READ window WRITE setWindow NOTIFY windowChanged)
public:
    WindowItem(QQuickItem *parent = nullptr) : QQuickItem(parent) {}
    ~WindowItem();

    Window *window() const;
    void setWindow(Window *window);
protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void windowChanged();
private:
    void createImpl(bool inProcess);
    void tearDown();

    struct Impl {
        Impl(WindowItem *windowItem) : q(windowItem) {}
        virtual ~Impl() {}
        virtual void setup(Window *window) = 0;
        virtual void tearDown() = 0;
        virtual void updateSize(const QSizeF &newSize) = 0;
        virtual bool isInProcess() const = 0;
        virtual Window *window() const = 0;
        WindowItem *q;
    };

    struct InProcessImpl : public Impl {
        InProcessImpl(WindowItem *windowItem) : Impl(windowItem) {}
        void setup(Window *window) override;
        void tearDown() override;
        void updateSize(const QSizeF &newSize) override;
        bool isInProcess() const override { return true; }
        Window *window() const override;

        InProcessWindow *m_inProcessWindow{nullptr};
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
        void createWaylandItem();

        WaylandWindow *m_waylandWindow{nullptr};
        QWaylandQuickItem *m_waylandItem{nullptr};
    };
#endif // AM_MULTI_PROCESS

    Impl *m_impl{nullptr};
};

QT_END_NAMESPACE_AM
