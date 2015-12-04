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

#include "abstractruntime.h"

class FakeApplicationManagerWindow;
class QmlInProcessApplicationInterface;

class QmlInProcessRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    QmlInProcessRuntimeManager(const QString &id, QObject *parent = 0);

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

    State state() const;
    void openDocument(const QString &document) override;
    qint64 applicationProcessId() const override;

public slots:
    bool start();
    void stop(bool forceKill = false);

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
