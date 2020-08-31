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

#include <QtAppManManager/abstractruntime.h>

#include <QSharedPointer>

QT_BEGIN_NAMESPACE_AM

class QmlInProcessApplicationManagerWindow;
class QmlInProcessApplicationInterface;
class InProcessSurfaceItem;

class QmlInProcessRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit QmlInProcessRuntimeManager(QObject *parent = nullptr);
    explicit QmlInProcessRuntimeManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();
    bool inProcess() const override;

    AbstractRuntime *create(AbstractContainer *container, Application *app) override;
};


class QmlInProcessRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit QmlInProcessRuntime(Application *app, QmlInProcessRuntimeManager *manager);
    ~QmlInProcessRuntime() override;

    void openDocument(const QString &document, const QString &mimeType) override;
    qint64 applicationProcessId() const override;

    static QmlInProcessRuntime *determineRuntime(QObject *object);

public slots:
    bool start() override;
    void stop(bool forceKill = false) override;
#if !defined(AM_HEADLESS)
    void stopIfNoVisibleSurfaces();
#endif

signals:
    void aboutToStop(); // used for the ApplicationInterface

private slots:
    void finish(int exitCode, Am::ExitStatus status);
#if !defined(AM_HEADLESS)
    void onSurfaceItemReleased(InProcessSurfaceItem*);
#endif

private:
    static const char *s_runtimeKey;

    QString m_document;
    QmlInProcessApplicationInterface *m_applicationIf = nullptr;

    bool m_stopIfNoVisibleSurfaces = false;

    void loadResources(const QStringList &resources, const QString &baseDir);
    void addPluginPaths(const QStringList &pluginPaths, const QString &baseDir);
    void addImportPaths(const QStringList &importPaths, const QString &baseDir);

#if !defined(AM_HEADLESS)
    // used by QmlInProcessApplicationManagerWindow to register surfaceItems
    void addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface);

    bool hasVisibleSurfaces() const;

    QObject *m_rootObject = nullptr;
    QList< QSharedPointer<InProcessSurfaceItem> > m_surfaces;

    friend class QmlInProcessApplicationManagerWindow; // for emitting signals on behalf of this class in onComplete
#endif
};

QT_END_NAMESPACE_AM
