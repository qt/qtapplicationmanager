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

#include <functional>

#include <QAbstractListModel>

#if defined(QT_DBUS_LIB)
#  include <QDBusContext>
#  include <QDBusConnectionInterface>
#endif

#if defined(AM_MULTI_PROCESS)
QT_FORWARD_DECLARE_CLASS(QWaylandSurface)
#endif

QT_FORWARD_DECLARE_CLASS(QQuickWindow)
QT_FORWARD_DECLARE_CLASS(QQuickItem)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)
QT_FORWARD_DECLARE_CLASS(QWindow)

class Window;
class WindowManagerPrivate;
class Application;
class AbstractRuntime;
class WaylandCompositor;

#if defined(AM_MULTI_PROCESS)

class WindowSurface
{
public:
    virtual ~WindowSurface() {}
    QWaylandSurface *surface() const { return m_surface; }

    virtual QQuickItem *item() const = 0;

    virtual void takeFocus() = 0;
    virtual void ping() = 0;
    virtual qint64 processId() const = 0;
    virtual QWindow *outputWindow() const = 0;

    virtual QVariantMap windowProperties() const = 0;
    virtual void setWindowProperty(const QString &name, const QVariant &value) = 0;

    virtual void connectPong(const std::function<void ()> &cb) = 0;
    virtual void connectWindowPropertyChanged(const std::function<void (const QString &name, const QVariant &value)> &cb) = 0;

protected:
    QWaylandSurface *m_surface;
};

#endif

class WindowManager : public QAbstractListModel
#if defined(QT_DBUS_LIB)
                                               , protected QDBusContext
#endif
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.WindowManager")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool runningOnDesktop READ isRunningOnDesktop CONSTANT)
public:
    ~WindowManager();
    static WindowManager *createInstance(QQmlEngine *qmlEngine, bool forceSingleProcess, const QString &waylandSocketName = QString());
    static WindowManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    bool isRunningOnDesktop() const;

    void enableWatchdog(bool enable);
    bool isWatchdogEnabled() const;

    QVector<const Window *> windows() const;

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;

    Q_INVOKABLE int indexOfWindow(QQuickItem *window);

    Q_INVOKABLE void releaseWindow(QQuickItem *window);

    Q_INVOKABLE void registerCompositorView(QQuickWindow *view);

signals:
    void countChanged();
    void raiseApplicationWindow(const QString &applicationId, const QString &applicationAliasId);

    void windowReady(int index, QQuickItem *window);
    void windowClosing(int index, QQuickItem *window);
    void windowLost(int index, QQuickItem *window);

    void windowPropertyChanged(QQuickItem *window, const QString &name, const QVariant &value);

private slots:
    void surfaceFullscreenChanged(QQuickItem *surfaceItem, bool isFullscreen);

    void inProcessSurfaceItemCreated(QQuickItem *surfaceItem);
    void setupWindow(Window *window);

public:
    Q_INVOKABLE bool setWindowProperty(QQuickItem *window, const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(QQuickItem *window, const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties(QQuickItem *window) const;

    Q_SCRIPTABLE bool makeScreenshot(const QString &filename, const QString &selector);

    bool setDBusPolicy(const QVariantMap &yamlFragment);

public slots:
    void setupInProcessRuntime(AbstractRuntime *runtime); // evil hook to support in-process runtimes

private slots:
    void reportFps();

#if defined(AM_MULTI_PROCESS)
private slots:
    void waylandSurfaceCreated(WindowSurface *surface);
    void waylandSurfaceMapped(WindowSurface *surface);
    void waylandSurfaceUnmapped(WindowSurface *surface);
    void waylandSurfaceDestroyed(WindowSurface *surface);

    void resize();

private:
    void handleWaylandSurfaceDestroyedOrUnmapped(QWaylandSurface *surface);
#endif

private:
    WindowManager(QQmlEngine *qmlEngine, bool forceSingleProcess, const QString &waylandSocketName);
    WindowManager(const WindowManager &);
    WindowManager &operator=(const WindowManager &);
    static WindowManager *s_instance;

    WindowManagerPrivate *d;

    friend class WaylandCompositor;
};
