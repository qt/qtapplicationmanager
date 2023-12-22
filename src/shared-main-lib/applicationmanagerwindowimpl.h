// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>
#include <QtGui/QColor>
#include <QtGui/QWindow>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQuickItem)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindow;
class ApplicationManagerWindowAttached;


class ApplicationManagerWindowImpl
{
public:
    static void setFactory(const std::function<ApplicationManagerWindowImpl *(ApplicationManagerWindow *)> &factory);

    static ApplicationManagerWindowImpl *create(ApplicationManagerWindow *window);

    virtual ~ApplicationManagerWindowImpl() = default;

    ApplicationManagerWindow *amWindow();

    virtual bool isInProcess() const = 0;
    virtual QObject *backingObject() const = 0;

    virtual void classBegin() = 0;
    virtual void componentComplete() = 0;

    virtual QQuickItem *contentItem() = 0;

    virtual QString title() const = 0;
    virtual void setTitle(const QString &title) = 0;
    virtual int x() const = 0;
    virtual void setX(int x) = 0;
    virtual int y() const = 0;
    virtual void setY(int y) = 0;
    virtual int width() const = 0;
    virtual void setWidth(int w) = 0;
    virtual int height() const = 0;
    virtual void setHeight(int h) = 0;
    virtual int minimumWidth() const = 0;
    virtual void setMinimumWidth(int minw) = 0;
    virtual int minimumHeight() const = 0;
    virtual void setMinimumHeight(int minh) = 0;
    virtual int maximumWidth() const = 0;
    virtual void setMaximumWidth(int maxw) = 0;
    virtual int maximumHeight() const = 0;
    virtual void setMaximumHeight(int maxh) = 0;
    virtual bool isVisible() const = 0;
    virtual void setVisible(bool visible) = 0;
    virtual qreal opacity() const = 0;
    virtual void setOpacity(qreal opacity) = 0;
    virtual QColor color() const = 0;
    virtual void setColor(const QColor &c) = 0;

    virtual bool isActive() const = 0;
    virtual QQuickItem *activeFocusItem() const = 0;

    virtual bool setWindowProperty(const QString &name, const QVariant &value) = 0;
    virtual QVariant windowProperty(const QString &name) const = 0;
    virtual QVariantMap windowProperties() const = 0;

    virtual void close() = 0;
    virtual QWindow::Visibility visibility() const = 0;
    virtual void setVisibility(QWindow::Visibility visibility) = 0;
    virtual void hide() = 0;
    virtual void show() = 0;
    virtual void showFullScreen() = 0;
    virtual void showMinimized() = 0;
    virtual void showMaximized() = 0;
    virtual void showNormal() = 0;

protected:
    ApplicationManagerWindowImpl(ApplicationManagerWindow *window);

private:
    ApplicationManagerWindow *m_amwindow = nullptr;

    static std::function<ApplicationManagerWindowImpl *(ApplicationManagerWindow *)> s_factory;
    Q_DISABLE_COPY_MOVE(ApplicationManagerWindowImpl)
};


class ApplicationManagerWindowAttachedImpl
{
public:
    static void setFactory(const std::function<ApplicationManagerWindowAttachedImpl *(ApplicationManagerWindowAttached *, QQuickItem *)> &factory);

    static ApplicationManagerWindowAttachedImpl *create(ApplicationManagerWindowAttached *window,
                                                        QQuickItem *attacheeItem);

    virtual ~ApplicationManagerWindowAttachedImpl() = default;

    ApplicationManagerWindowAttached *amWindowAttached() const;
    QQuickItem *attacheeItem() const;

    virtual ApplicationManagerWindow *findApplicationManagerWindow() = 0;
    void onWindowChanged(ApplicationManagerWindow *newWin);

protected:
    ApplicationManagerWindowAttachedImpl(ApplicationManagerWindowAttached *windowAttached,
                                         QQuickItem *attacheeItem);

private:
    ApplicationManagerWindowAttached *m_amWindowAttached;
    QQuickItem *m_attacheeItem;

    static std::function<ApplicationManagerWindowAttachedImpl *(ApplicationManagerWindowAttached *, QQuickItem *)> s_factory;
    Q_DISABLE_COPY_MOVE(ApplicationManagerWindowAttachedImpl)
};

QT_END_NAMESPACE_AM
