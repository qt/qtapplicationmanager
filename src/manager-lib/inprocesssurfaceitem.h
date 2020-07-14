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

#if !defined(AM_HEADLESS)

#include <QColor>
#include <QQuickItem>
#include <QPointer>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

/*
 *  Item exposed to the System UI
 */
class InProcessSurfaceItem : public QQuickItem
{
    Q_OBJECT
public:
    InProcessSurfaceItem(QQuickItem *parent = nullptr);
    ~InProcessSurfaceItem() override;

    bool setWindowProperty(const QString &name, const QVariant &value);
    QVariant windowProperty(const QString &name) const;
    QObject &windowProperties() { return m_windowProperties; }
    QVariantMap windowPropertiesAsVariantMap() const;

    // Whether the surface is visible on the client (application) side.
    // The when application hides its surface, System UI should, in an animated way,
    // remove it from user's view.
    // That's why a client shouldn't have control over the actual visible property
    // of its surface item.
    void setVisibleClientSide(bool);
    bool visibleClientSide() const;

    QColor color() const;
    void setColor(const QColor &c);

    void close();

    QObject *inProcessApplicationManagerWindow() { return m_inProcessAppManWindow.data(); }
    void setInProcessApplicationManagerWindow(QObject* win) { m_inProcessAppManWindow = win; }

signals:
    void visibleClientSideChanged();
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void closeRequested();

    // Emitted when WindowManager is no longer tracking this surface item
    // (ie, the Window that was tracking this surface item has been destroyed)
    void released();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    QObject m_windowProperties;
    bool m_visibleClientSide = true;
    QColor m_color;
    QPointer<QObject> m_inProcessAppManWindow;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(InProcessSurfaceItem*))
Q_DECLARE_METATYPE(QSharedPointer<QT_PREPEND_NAMESPACE_AM(InProcessSurfaceItem)>)

#endif  // !AM_HEADLESS
