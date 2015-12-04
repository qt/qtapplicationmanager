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

#pragma once

#include <QQuickWindow>
#include <QTime>

QT_FORWARD_DECLARE_CLASS(QPlatformWindow)


class ApplicationManagerWindowPrivate;

class ApplicationManagerWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(float fps READ fps NOTIFY fpsChanged)

public:
    explicit ApplicationManagerWindow(QWindow *parent = 0);

    float fps() const;

    Q_INVOKABLE bool setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;

signals:
    void fpsChanged(float fps);
    void windowPropertyChanged(const QString &name, const QVariant &value);

private slots:
    void onFrameSwapped();
    void onWindowPropertyChangedInternal(QPlatformWindow *window, const QString &name);

private:
    ApplicationManagerWindowPrivate *d;
};
