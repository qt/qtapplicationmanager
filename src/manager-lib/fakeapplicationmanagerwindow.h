/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QColor>
#include <QQuickItem>
#include "global.h"

class QmlInProcessRuntime;


class FakeApplicationManagerWindow : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor) // dummy to mimick Window's API

public:
    explicit FakeApplicationManagerWindow(QQuickItem *parent = 0);
    ~FakeApplicationManagerWindow();

    QColor color() const { return QColor(); }
    void setColor(const QColor &) { }

    Q_INVOKABLE bool setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;

    void componentComplete() override;

public slots:
    void close();
    void showFullScreen();
    void showMaximized();
    void showNormal();
    //    bool close() { emit fakeCloseSignal(); return true; } // not supported right now, because it causes crashes in multiprocess mode
    //... revisit later (after andies resize-redesign) to check if close() is working for wayland

    // following QWindow slots aren't implemented yet:
    //    void hide()
    //    void lower()
    //    void raise()
    //    void setHeight(int arg)
    //    void setMaximumHeight(int h)
    //    void setMaximumWidth(int w)
    //    void setMinimumHeight(int h)
    //    void setMinimumWidth(int w)
    //    void setTitle(const QString &)
    //    void setVisible(bool visible)
    //    void setWidth(int arg)
    //    void setX(int arg)
    //    void setY(int arg)
    //    void show()
    //    void showMinimized()

signals:
    void fakeCloseSignal();
    void fakeFullScreenSignal();
    void fakeNoFullScreenSignal(); // TODO this should be replaced by 'normal' and 'maximized' as soon as needed
    void windowPropertyChanged(const QString &name, const QVariant &value);

protected:
    bool event(QEvent *e) override;

private:
    QmlInProcessRuntime *m_runtime;

    friend class QmlInProcessRuntime; // for setting the m_runtime member
};

