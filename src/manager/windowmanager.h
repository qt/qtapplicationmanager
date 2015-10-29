/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
****************************************************************************/

#pragma once

#include <QObject>
#include <QUrl>
#include <QAbstractListModel>

#ifndef AM_SINGLEPROCESS_MODE
#  if QT_VERSION < QT_VERSION_CHECK(5,3,0)
#    include <QWaylandCompositor>
#    include <QWaylandSurface>
#  else
#    include <QWaylandQuickCompositor>
#    include <QWaylandQuickSurface>
#  endif
#  if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
#    include <QWaylandClient>
#  endif
#  include <QWaylandSurfaceItem>
#  include <QProcess>
#endif


class QQuickView;
class QQuickItem;
class QQmlEngine;
class QJSEngine;
class Window;
class WindowManagerPrivate;
class Application;
class AbstractRuntime;

class WindowManager : public QAbstractListModel
#ifndef AM_SINGLEPROCESS_MODE
#  if QT_VERSION < QT_VERSION_CHECK(5,3,0)
                                               , private QWaylandCompositor
#  else
                                               , private QWaylandQuickCompositor
#endif
#endif
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    ~WindowManager();
    static WindowManager *createInstance(QQuickView* view);
    static WindowManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    void enableWatchdog(bool enable);
    bool isWatchdogEnabled() const;

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE int count() const { return rowCount(); }
    Q_INVOKABLE QVariantMap get(int index) const;

    Q_INVOKABLE void releaseSurfaceItem(int index, QQuickItem* item); // after 'surfaceItemClosing' has been called, this function must be called to 'free' the surfaceItem (allow deleteLater) on it, after this has been called, surfaceItemLost should happen

signals:
    void countChanged();
    void raiseApplicationWindow(const QString &id);

    void surfaceItemReady(int index, QQuickItem* item);             // this is emitted after a new surfaceItem (/window) has been created (new application launch) or is otherwise ready (already running application got 'launched')
    void surfaceItemClosing(int index, QQuickItem* item);           // this is emitted when the application is about to close (e.g. process has killed himself and waylandsurface got unmapped/destroyed) or QQuickItem fired 'close'-signal (for qmlinprocess)
    void surfaceItemLost(int index, QQuickItem* item);              // after this signal has been fired, this QQuickItem is not usable anymore. This should only happen after releaseSurfaceItem has been called!
    // maybe more to come .. e.g. raise, hide, fullscreen(?) ...

    void surfaceWindowPropertyChanged(QQuickItem *surfaceItem, const QString &name, const QVariant &value);

public slots:
    void surfaceFullscreenChanged(QQuickItem *surfaceItem, bool isFullscreen);

    void inProcessSurfaceItemCreated(QQuickItem *surfaceItem);                       //TODO: check if still correct: calls surfaceItemCreated(QQuickItem *surfaceItem, const Application* app)
    void setupWindow(Window *window);                                                //TODO: check if still correct: called after creating is complete. creating and registering all the created data (item and app); Also connect QObject::destroyed with WindowManger::surfaceItemDestroyed
    void surfaceItemAboutToClose(QQuickItem *item);                                  // called when application wants to close (either because the wayland_surface disconnected/unmapped or because in-process item fired corresponding signal ... the only thing it does is firing surfaceItemClosing ... qml has(!) to call releaseSurfaceItem somewhen after this!
    void surfaceItemDestroyed(QQuickItem* item);                                     // called after item got deleted (QObject::destroyed()). cleaning up the data-entries and firing corresponding signals

    void setupInProcessRuntime(AbstractRuntime *runtime); // evil hook to support in-process runtimes

public:
    Q_INVOKABLE bool setSurfaceWindowProperty(QQuickItem *item, const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant surfaceWindowProperty(QQuickItem *item, const QString &name) const;
    Q_INVOKABLE QVariantMap surfaceWindowProperties(QQuickItem *item) const;

private slots:
    void windowPropertyChanged(const QString &name, const QVariant &value);

#ifndef AM_SINGLEPROCESS_MODE
public slots:
    void surfaceCreated(QWaylandSurface *surface);

private slots:
    void waylandSurfaceMapped();
    void waylandSurfaceUnmapped();
    void waylandSurfaceDestroyed();

    void sendCallbacks();

    void resize();

private:
    void handleWaylandSurfaceDestroyedOrUnmapped(QWaylandSurface *surface);
#endif

private:
    WindowManager(QQuickView *parent = 0);
    WindowManager(const WindowManager &);
    WindowManager &operator=(const WindowManager &);
    static WindowManager *s_instance;

    WindowManagerPrivate *d;
};
