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
#include "fakepelagicorewindow.h"
#include "qmlinprocessruntime.h"


FakePelagicoreWindow::FakePelagicoreWindow(QQuickItem *parent)
    : QQuickItem(parent)
    , m_runtime(0)
{
    qCDebug(LogSystem) << "FakePelagicoreWindow ctor! this:" << this;

}

FakePelagicoreWindow::~FakePelagicoreWindow()
{
    qCDebug(LogSystem) << "FakePelagicoreWindow dtor! this: " << this;
}

void FakePelagicoreWindow::close()
{
    emit fakeCloseSignal();
}

void FakePelagicoreWindow::showFullScreen()
{
    emit fakeFullScreenSignal();
}
void FakePelagicoreWindow::showMaximized()
{
    emit fakeNoFullScreenSignal();
}

void FakePelagicoreWindow::showNormal()
{
    // doesn't work in wayland right now, so do nothing... revisit later (after andies resize-redesign)
}



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


bool FakePelagicoreWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed) {
        setProperty(key, value);
    }
    return true;
}

QVariant FakePelagicoreWindow::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return property(key);
}

QVariantMap FakePelagicoreWindow::windowProperties() const
{
    QList<QByteArray> keys = dynamicPropertyNames();
    QVariantMap map;

    foreach (const QByteArray &key, keys) {
        if (!isName(key))
            continue;

        QString name = keyToName(key);
        map[name] = property(key);
    }

    return map;
}

bool FakePelagicoreWindow::event(QEvent *e)
{
    if (e->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
        QByteArray key = dpce->propertyName();

        if (isName(key)) {
            QString name = keyToName(dpce->propertyName());
            emit windowPropertyChanged(name, property(key));

            //qWarning() << "FPW: got change" << name << " --> " << property(key).toString();
        }
    }

    return QQuickItem::event(e);
}

void FakePelagicoreWindow::componentComplete()
{
    qCDebug(LogSystem) << "FakePelagicoreWindow componentComplete() this:" << this;

    QQuickItem::componentComplete();

    QQuickItem *parent = parentItem();

    while (parent && !m_runtime) {
        if (FakePelagicoreWindow *windowParent = qobject_cast<FakePelagicoreWindow *>(parent)) {
            m_runtime = windowParent->m_runtime;
        }
        parent = parent->parentItem();
    }

    if (m_runtime)
        m_runtime->addWindow(this);
}
