/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QtAppManManager/abstractruntime.h>

QT_BEGIN_NAMESPACE_AM

class FakeApplicationManagerWindow;
class QmlInProcessApplicationInterface;

class QmlInProcessRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit QmlInProcessRuntimeManager(QObject *parent = 0);
    explicit QmlInProcessRuntimeManager(const QString &id, QObject *parent = 0);

    static QString defaultIdentifier();
    bool inProcess() const override;

    AbstractRuntime *create(AbstractContainer *container, const Application *app) override;
};


class QmlInProcessRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit QmlInProcessRuntime(const Application *app, QmlInProcessRuntimeManager *manager);
    ~QmlInProcessRuntime();

    void openDocument(const QString &document) override;
    qint64 applicationProcessId() const override;

public slots:
    bool start() override;
    void stop(bool forceKill = false) override;

signals:
    void aboutToStop(); // used for the ApplicationInterface

private slots:
#if !defined(AM_HEADLESS)
    void onWindowClose();
    void onWindowDestroyed();

    void onEnableFullscreen();
    void onDisableFullscreen();
#endif

private:
    QString m_document;
    QmlInProcessApplicationInterface *m_applicationIf = 0;

#if !defined(AM_HEADLESS)
    // used by FakeApplicationManagerWindow to register windows
    void addWindow(QQuickItem *window);

    FakeApplicationManagerWindow *m_mainWindow = 0;
    QList<QQuickItem *> m_windows;

    friend class FakeApplicationManagerWindow; // for emitting signals on behalf of this class in onComplete
#endif
};

QT_END_NAMESPACE_AM
