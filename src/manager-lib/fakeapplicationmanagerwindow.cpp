/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/
#include "fakeapplicationmanagerwindow.h"
#include "qmlinprocessruntime.h"


FakeApplicationManagerWindow::FakeApplicationManagerWindow(QQuickItem *parent)
    : QQuickItem(parent)
    , m_runtime(0)
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow ctor! this:" << this;

}

FakeApplicationManagerWindow::~FakeApplicationManagerWindow()
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow dtor! this: " << this;
}

void FakeApplicationManagerWindow::close()
{
    emit fakeCloseSignal();
}

void FakeApplicationManagerWindow::showFullScreen()
{
    emit fakeFullScreenSignal();
}
void FakeApplicationManagerWindow::showMaximized()
{
    emit fakeNoFullScreenSignal();
}

void FakeApplicationManagerWindow::showNormal()
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


bool FakeApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed) {
        setProperty(key, value);
    }
    return true;
}

QVariant FakeApplicationManagerWindow::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return property(key);
}

QVariantMap FakeApplicationManagerWindow::windowProperties() const
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

bool FakeApplicationManagerWindow::event(QEvent *e)
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

void FakeApplicationManagerWindow::componentComplete()
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow componentComplete() this:" << this;

    QQuickItem::componentComplete();

    QQuickItem *parent = parentItem();

    while (parent && !m_runtime) {
        if (FakeApplicationManagerWindow *windowParent = qobject_cast<FakeApplicationManagerWindow *>(parent)) {
            m_runtime = windowParent->m_runtime;
        }
        parent = parent->parentItem();
    }

    if (m_runtime)
        m_runtime->addWindow(this);
}
