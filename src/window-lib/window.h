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

#include <QObject>
#include <QVariantMap>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class AbstractApplication;
class WindowItem;

// A Window object exists for every application window that is managed by the application-manager
// and has been shown (mapped) at least once.
// In the Wayland case, every Window is a surface, but not every surface is a Window. That's why
// there is a separate WindowSurface class in waylandcompositor.h.

class Window : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ContentState contentState READ contentState NOTIFY contentStateChanged)
    Q_PROPERTY(AbstractApplication* application READ application CONSTANT)
public:

    enum ContentState {
        SurfaceWithContent,
        SurfaceNoContent,
        NoSurface
    };
    Q_ENUM(ContentState)

    Window(AbstractApplication *app);
    virtual ~Window();

    virtual bool isInProcess() const = 0;
    virtual AbstractApplication *application() const;

    // Controls how many items (which are views from a model-view perspective) are currently rendering this window
    void registerItem(WindowItem *item);
    void unregisterItem(WindowItem *item);
    const QSet<WindowItem*> &items() const { return m_items; }

    void setPrimaryItem(WindowItem *item);
    WindowItem *primaryItem() const { return m_primaryItem; }

    // Whether there's any view (WindowItem) holding a reference to this window
    bool isBeingDisplayed() const;

    virtual ContentState contentState() const = 0;

    Q_INVOKABLE virtual bool setWindowProperty(const QString &name, const QVariant &value) = 0;
    Q_INVOKABLE virtual QVariant windowProperty(const QString &name) const = 0;
    Q_INVOKABLE virtual QVariantMap windowProperties() const = 0;

signals:
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void isBeingDisplayedChanged();
    void contentStateChanged();

protected:
    AbstractApplication *m_application;

    QSet<WindowItem*> m_items;
    WindowItem *m_primaryItem{nullptr};
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(Window*))
