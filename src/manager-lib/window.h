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

#include <QObject>
#include <QVariantMap>
#include <QPointer>
#include <QQuickItem>

class Application;
class WindowPrivate;

class Window : public QObject
{
    Q_OBJECT

public:
    Window(const Application *app, QQuickItem *surfaceItem);
    virtual ~Window();

    virtual bool isInProcess() const = 0;
    virtual QQuickItem *surfaceItem() const;
    virtual const Application *application() const;

    virtual void setClosing();
    virtual bool isClosing() const;

    virtual bool setWindowProperty(const QString &name, const QVariant &value) = 0;
    virtual QVariant windowProperty(const QString &name) const = 0;
    virtual QVariantMap windowProperties() const = 0;

    // we cannot directly derive from QObject here, so we have to fake connect/disconnect
    void connectWindowPropertyChanged(QObject *target, const char *member, Qt::ConnectionType type = Qt::AutoConnection);
    void disconnectWindowPropertyChanged(QObject *target, const char *member = 0);

signals:
    void windowPropertyChanged(const QString &name, const QVariant &value);

protected:
    const Application *m_application;
    QPointer<QQuickItem> m_surfaceItem;
    bool m_isClosing = false;
};

