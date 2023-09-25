// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>
#include <QtCore/QAbstractListModel>
#include <QtAppManCommon/global.h>

#if defined(Q_MOC_RUN) && !defined(__attribute__) && !defined(__declspec)
#  define QT_PREPEND_NAMESPACE_AM(name) QtAM::name
#  error "This pre-processor scope is for qdbuscpp2xml only, but it seems something else triggered it"
#endif

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
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/WindowManager 2.0 SINGLETON")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool runningOnDesktop READ isRunningOnDesktop CONSTANT FINAL)
    Q_PROPERTY(bool slowAnimations READ slowAnimations WRITE setSlowAnimations NOTIFY slowAnimationsChanged FINAL)
    Q_PROPERTY(bool allowUnknownUiClients READ allowUnknownUiClients CONSTANT FINAL)

public:
    ~WindowManager() override;
    static WindowManager *createInstance(QQmlEngine *qmlEngine, const QString &waylandSocketName = QString());
    static WindowManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

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
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Window) *window(int index) const;
    Q_INVOKABLE QList<QT_PREPEND_NAMESPACE_AM(Window) *> windowsOfApplication(const QString &id) const;
    Q_INVOKABLE int indexOfWindow(QT_PREPEND_NAMESPACE_AM(Window) *window) const;
    Q_INVOKABLE QObject *addExtension(QQmlComponent *component) const;

    WindowManagerInternalSignals internalSignals;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    Q_SCRIPTABLE void countChanged();
    Q_SCRIPTABLE void slowAnimationsChanged(bool);

    void raiseApplicationWindow(const QString &applicationId, const QString &applicationAliasId);

    void windowAdded(QT_PREPEND_NAMESPACE_AM(Window) *window);
    void windowAboutToBeRemoved(QT_PREPEND_NAMESPACE_AM(Window) *window);
    void windowContentStateChanged(QT_PREPEND_NAMESPACE_AM(Window) *window);

    void windowPropertyChanged(QT_PREPEND_NAMESPACE_AM(Window) *window, const QString &name, const QVariant &value);

private slots:
    void inProcessSurfaceItemCreated(QT_PREPEND_NAMESPACE_AM(AbstractRuntime) *runtime,
                                     QSharedPointer<QT_PREPEND_NAMESPACE_AM(InProcessSurfaceItem)> surfaceItem);
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
