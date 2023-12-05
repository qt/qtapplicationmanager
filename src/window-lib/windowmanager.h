// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>
#include <QtCore/QAbstractListModel>
#include <QtAppManCommon/global.h>


#if QT_CONFIG(am_multi_process)
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


// A place to collect signals used internally by appman without polluting
// WindowManager's public QML API.
class WindowManagerInternalSignals : public QObject
{
    Q_OBJECT
signals:
    // Emitted right before the WaylandCompositor instance is created
    void compositorAboutToBeCreated();
    void shutDownFinished();
};

class WindowManager : public QAbstractListModel

{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.qt.WindowManager")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool runningOnDesktop READ isRunningOnDesktop CONSTANT FINAL)
    Q_PROPERTY(bool slowAnimations READ slowAnimations WRITE setSlowAnimations NOTIFY slowAnimationsChanged FINAL)
    Q_PROPERTY(bool allowUnknownUiClients READ allowUnknownUiClients CONSTANT FINAL)

public:
    ~WindowManager() override;
    static WindowManager *createInstance(QQmlEngine *qmlEngine, const QString &waylandSocketName = QString());
    static WindowManager *instance();

    void shutDown();

    bool isRunningOnDesktop() const;
    bool slowAnimations() const;
    void setSlowAnimations(bool slowAnimations);
    bool allowUnknownUiClients() const;
    void setAllowUnknownUiClients(bool enable);
    void enableWatchdog(bool enable);

    bool addWaylandSocket(QLocalServer *waylandSocket);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QtAM::Window *window(int index) const;
    Q_INVOKABLE QList<QtAM::Window *> windowsOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfWindow(QtAM::Window *window) const;
    Q_INVOKABLE QObject *addExtension(QQmlComponent *component) const;

    WindowManagerInternalSignals internalSignals;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    Q_SCRIPTABLE void countChanged();
    Q_SCRIPTABLE void slowAnimationsChanged(bool);

    void raiseApplicationWindow(const QString &applicationId, const QString &applicationAliasId);

    void windowAdded(QtAM::Window *window);
    void windowAboutToBeRemoved(QtAM::Window *window);
    void windowContentStateChanged(QtAM::Window *window);

    void windowPropertyChanged(QtAM::Window *window, const QString &name, const QVariant &value);

private slots:
    void inProcessSurfaceItemCreated(QtAM::AbstractRuntime *runtime,
                                     QSharedPointer<QtAM::InProcessSurfaceItem> surfaceItem);
    void setupWindow(QtAM::Window *window);

public:
    Q_SCRIPTABLE bool makeScreenshot(const QString &filename, const QString &selector);

    QList<QQuickWindow *> compositorViews() const;

    // evil hook to support in-process runtimes
    void setupInProcessRuntime(QtAM::AbstractRuntime *runtime);

#if QT_CONFIG(am_multi_process)
private slots:
    void waylandSurfaceCreated(QWaylandSurface *surface);
    void waylandSurfaceMapped(QtAM::WindowSurface *surface);

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
