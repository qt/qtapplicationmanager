/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
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

#include "inprocesswindow.h"

static QByteArray nameToKey(const QString &name)
{
    return QByteArray("_am_") + name.toUtf8();
}

static QString keyToName(const QByteArray &key)
{
    return QString::fromUtf8(key.mid(4));
}

static bool isName(const QByteArray &key)
{
    return key.startsWith("_am_");
}


InProcessWindow::InProcessWindow(const Application *app, QQuickItem *surfaceItem)
    : Window(app, surfaceItem)
{
    surfaceItem->installEventFilter(this);
}

bool InProcessWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = surfaceItem()->property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed) {
        surfaceItem()->setProperty(key, value);
    }
    return true;
}

QVariant InProcessWindow::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return surfaceItem()->property(key);
}

QVariantMap InProcessWindow::windowProperties() const
{
    QList<QByteArray> keys = surfaceItem()->dynamicPropertyNames();
    QVariantMap map;

    foreach (const QByteArray &key, keys) {
        if (!isName(key))
            continue;

        QString name = QString::fromUtf8(key.mid(4));
        map[name] = surfaceItem()->property(key);
    }

    return map;
}

bool InProcessWindow::eventFilter(QObject *o, QEvent *e)
{
    if ((o == surfaceItem()) && (e->type() == QEvent::DynamicPropertyChange)) {
        QDynamicPropertyChangeEvent *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
        QByteArray key = dpce->propertyName();

        if (isName(key)) {
            QString name = keyToName(dpce->propertyName());
            emit windowPropertyChanged(name, surfaceItem()->property(key));

            //qWarning() << "IPW: got change" << name << " --> " << surfaceItem()->property(key).toString();
        }
    }

    return Window::eventFilter(o, e);
}
