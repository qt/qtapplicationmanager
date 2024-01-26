// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/applicationmanagerwindowimpl.h>


QT_BEGIN_NAMESPACE_AM

class ApplicationMain;
class AMQuickWindowQmlImpl;

class WaylandApplicationManagerWindowImpl : public ApplicationManagerWindowImpl
{
public:
    WaylandApplicationManagerWindowImpl(ApplicationManagerWindow *window, QtAM::ApplicationMain *applicationMain);
    ~WaylandApplicationManagerWindowImpl() override;

    bool isSingleProcess() const override;
    QObject *backingObject() const override;

    void classBegin() override;
    void componentComplete() override;

    QQuickItem *contentItem() override;

    QString title() const override;
    void setTitle(const QString &title) override;
    int x() const override;
    void setX(int x) override;
    int y() const override;
    void setY(int y) override;
    int width() const override;
    void setWidth(int w) override;
    int height() const override;
    void setHeight(int h) override;
    int minimumWidth() const override;
    void setMinimumWidth(int minw) override;
    int minimumHeight() const override;
    void setMinimumHeight(int minh) override;
    int maximumWidth() const override;
    void setMaximumWidth(int maxw) override;
    int maximumHeight() const override;
    void setMaximumHeight(int maxh) override;
    bool isVisible() const override;
    void setVisible(bool visible) override;
    qreal opacity() const override;
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
    QPointer<ApplicationMain> m_applicationMain;
    QPointer<AMQuickWindowQmlImpl> m_qwindow;
    QColor m_color;
};


class WaylandApplicationManagerWindowAttachedImpl : public ApplicationManagerWindowAttachedImpl
{
public:
    WaylandApplicationManagerWindowAttachedImpl(ApplicationManagerWindowAttached *windowAttached,
                                                QQuickItem *attacheeItem);

    ApplicationManagerWindow *findApplicationManagerWindow() override;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
