// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QSharedPointer>
#include <QtGui/QColor>
#include <QtQml/QQmlComponent>
#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/applicationmanagerwindowimpl.h>


QT_FORWARD_DECLARE_CLASS(QQuickItem)

QT_BEGIN_NAMESPACE_AM

class InProcessSurfaceItem;
class QmlInProcRuntime;
class ApplicationManagerWindow;


class QmlInProcApplicationManagerWindowImpl : public ApplicationManagerWindowImpl
{
public:
    QmlInProcApplicationManagerWindowImpl(ApplicationManagerWindow *window);
    ~QmlInProcApplicationManagerWindowImpl() override;

    bool isInProcess() const override;
    QObject *backingObject() const override;

    void classBegin() override;
    void componentComplete() override;

    QQuickItem *contentItem() override;

    QString title() const override { return m_title; }
    void setTitle(const QString &title) override;
    int x() const override { return m_x; }
    void setX(int x) override;
    int y() const override { return m_y; }
    void setY(int y) override;
    int width() const override;
    void setWidth(int w) override;
    int height() const override;
    void setHeight(int h) override;
    int minimumWidth() const override { return m_minimumWidth; }
    void setMinimumWidth(int minw) override;
    int minimumHeight() const override { return m_minimumHeight; }
    void setMinimumHeight(int minh) override;
    int maximumWidth() const override { return m_maximumWidth; }
    void setMaximumWidth(int maxw) override;
    int maximumHeight() const override { return m_maximumHeight; }
    void setMaximumHeight(int maxh) override;
    bool isVisible() const override;
    void setVisible(bool visible) override;
    qreal opacity() const override { return m_opacity; }
    void setOpacity(qreal opactity) override;
    QColor color() const override;
    void setColor(const QColor &c) override;
    bool isActive() const override;
    QQuickItem *activeFocusItem() const override;

    bool setWindowProperty(const QString &name, const QVariant &value) override;
    QVariant windowProperty(const QString &name) const override;
    QVariantMap windowProperties() const override;
    void close() override;
    QWindow::Visibility visibility() const override;
    void setVisibility(QWindow::Visibility newVisibility) override;
    void hide() override;
    void show() override;
    void showFullScreen() override;
    void showMinimized() override;
    void showMaximized() override;
    void showNormal() override;

private:
    void notifyRuntimeAboutSurface();
    void determineRuntime();
    void findParentWindow(QObject *object);
    void setRelations(ApplicationManagerWindow *newParentWindow);
    void connectActiveFocusItem();
    void updateVisibility(QWindow::Visibility newVisibility);

    static QVector<QmlInProcApplicationManagerWindowImpl *> s_inCompleteWindows;

    QSharedPointer<InProcessSurfaceItem> m_surfaceItem;
    QmlInProcRuntime *m_runtime = nullptr;
    QVector<QQmlComponentAttached *> m_attachedCompleteHandlers;
    ApplicationManagerWindow *m_parentWindow = nullptr;
    QVector<ApplicationManagerWindow*> m_childWindows;
    QMetaObject::Connection m_parentVisibleConnection;
    QMetaObject::Connection m_activeFocusItemConnection;

private:
    QString m_title;
    int m_x = 0;
    int m_y = 0;
    int m_minimumWidth = 0;
    int m_minimumHeight = 0;
    static const int WindowSizeMax = (1 << 24) - 1; // same as QWINDOWSIZE_MAX
    int m_maximumWidth = WindowSizeMax;
    int m_maximumHeight = WindowSizeMax;
    qreal m_opacity = 1;
    QQuickItem *m_activeFocusItem = nullptr;
    QWindow::Visibility m_visibility = QWindow::Hidden;

    friend class QmlInProcRuntime; // for setting the m_runtime member
};


class QmlInProcApplicationManagerWindowAttachedImpl : public ApplicationManagerWindowAttachedImpl
{
public:
    QmlInProcApplicationManagerWindowAttachedImpl(ApplicationManagerWindowAttached *windowAttached,
                                                  QQuickItem *attacheeItem);

    ApplicationManagerWindow *findApplicationManagerWindow() override;

private:
    void onParentChanged();
    InProcessSurfaceItem *findInProcessSurfaceItem();

    QVector<QMetaObject::Connection> m_parentChangeConnections;
};

QT_END_NAMESPACE_AM
