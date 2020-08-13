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

#include <functional>
#include <QAbstractListModel>
#include <QtAppManCommon/global.h>

#if defined(AM_MULTI_PROCESS)
QT_FORWARD_DECLARE_CLASS(QWaylandSurface)
#endif

QT_FORWARD_DECLARE_CLASS(QQuickWindow)
QT_FORWARD_DECLARE_CLASS(QQuickItem)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)
QT_FORWARD_DECLARE_CLASS(QWindow)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)
QT_FORWARD_DECLARE_CLASS(QLocalServer)

QT_BEGIN_NAMESPACE_AM

class InProcessSurfaceItem;
class Window;
class WindowSurface;
class WindowManagerPrivate;
class Application;
class AbstractRuntime;
class WaylandCompositor;

class WindowManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.WindowManager")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/WindowManager 2.0 SINGLETON")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool runningOnDesktop READ isRunningOnDesktop CONSTANT)
    Q_PROPERTY(bool slowAnimations READ slowAnimations WRITE setSlowAnimations NOTIFY slowAnimationsChanged)

public:
    ~WindowManager() override;
    static WindowManager *createInstance(QQmlEngine *qmlEngine, const QString &waylandSocketName = QString());
    static WindowManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    void shutDown();

    bool isRunningOnDesktop() const;
    bool slowAnimations() const;
    void setSlowAnimations(bool slowAnimations);

    void enableWatchdog(bool enable);

    bool addWaylandSocket(QLocalServer *waylandSocket);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE Window *window(int index) const;
    Q_INVOKABLE QList<QObject *> windowsOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfWindow(Window *window) const;
    Q_INVOKABLE QObject *addExtension(QQmlComponent *component) const;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void countChanged();
    void raiseApplicationWindow(const QString &applicationId, const QString &applicationAliasId);

    void windowAdded(Window *window);
    void windowAboutToBeRemoved(Window *window);
    void windowContentStateChanged(Window *window);

    void windowPropertyChanged(Window *window, const QString &name, const QVariant &value);

    void compositorViewRegistered(QQuickWindow *view);

    void shutDownFinished();

    void slowAnimationsChanged(bool);

    void _inProcessSurfaceItemReleased(QSharedPointer<InProcessSurfaceItem>);

private slots:
    void inProcessSurfaceItemCreated(QSharedPointer<InProcessSurfaceItem> surfaceItem);
    void setupWindow(QT_PREPEND_NAMESPACE_AM(Window) *window);

public:
    Q_SCRIPTABLE bool makeScreenshot(const QString &filename, const QString &selector);

    QList<QQuickWindow *> compositorViews() const;

    // evil hook to support in-process runtimes
    void setupInProcessRuntime(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime);

#if defined(AM_MULTI_PROCESS)
private slots:
    void waylandSurfaceCreated(QWaylandSurface *surface);
    void waylandSurfaceMapped(QT_PREPEND_NAMESPACE_AM(WindowSurface) *surface);

private:
    void handleWaylandSurfaceDestroyedOrUnmapped(QWaylandSurface *surface);
#endif

private:
    void registerCompositorView(QQuickWindow *view);
    void addWindow(Window *window);
    void removeWindow(Window *window);
    void releaseWindow(Window *window);
    void updateViewSlowMode(QQuickWindow *view);
    WindowManager(QQmlEngine *qmlEngine, const QString &waylandSocketName);
    WindowManager(const WindowManager &);
    WindowManager &operator=(const WindowManager &);
    static WindowManager *s_instance;

    WindowManagerPrivate *d;

    friend class WaylandCompositor;
};

QT_END_NAMESPACE_AM
