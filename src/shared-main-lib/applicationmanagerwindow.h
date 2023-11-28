// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <memory>

#include <QtGui/QColor>
#include <QtGui/QWindow>
#include <QtQml/QQmlParserStatus>
#include <QtQml/QQmlListProperty>
#include <QtQml/QQmlEngine>
#include <QtAppManCommon/global.h>


QT_FORWARD_DECLARE_CLASS(QQmlComponentAttached)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindowImpl;
class ApplicationManagerWindowAttached;
class ApplicationManagerWindowAttachedImpl;


class ApplicationManagerWindow : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(bool inProcess READ isInProcess CONSTANT FINAL)
    Q_PROPERTY(QObject *backingObject READ backingObject CONSTANT FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(QQuickItem *contentItem READ contentItem CONSTANT FINAL)

    // QWindow properties
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged FINAL)
    Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged FINAL)
    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged FINAL)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged FINAL)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged FINAL)
    Q_PROPERTY(int minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged FINAL)
    Q_PROPERTY(int minimumHeight READ minimumHeight WRITE setMinimumHeight NOTIFY minimumHeightChanged FINAL)
    Q_PROPERTY(int maximumWidth READ maximumWidth WRITE setMaximumWidth NOTIFY maximumWidthChanged FINAL)
    Q_PROPERTY(int maximumHeight READ maximumHeight WRITE setMaximumHeight NOTIFY maximumHeightChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(QWindow::Visibility visibility READ visibility WRITE setVisibility NOTIFY visibilityChanged FINAL)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged FINAL)
    Q_PROPERTY(QQuickItem *activeFocusItem READ activeFocusItem NOTIFY activeFocusItemChanged FINAL)

    Q_PROPERTY(QQmlListProperty<QObject> data READ data NOTIFY dataChanged FINAL)

    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit ApplicationManagerWindow(QObject *parent = nullptr);
    ~ApplicationManagerWindow() override;

    static ApplicationManagerWindowAttached *qmlAttachedProperties(QObject *object);

    bool isInProcess() const;
    QObject *backingObject() const;

    QQmlListProperty<QObject> data();
    Q_SIGNAL void dataChanged();

    QQuickItem *contentItem();

    void classBegin() override;
    void componentComplete() override;

    QString title() const;
    void setTitle(const QString &title);
    Q_SIGNAL void titleChanged();
    int x() const;
    void setX(int x);
    Q_SIGNAL void xChanged();
    int y() const;
    void setY(int y);
    Q_SIGNAL void yChanged();
    int width() const;
    void setWidth(int w);
    Q_SIGNAL void widthChanged();
    int height() const;
    void setHeight(int h);
    Q_SIGNAL void heightChanged();
    int minimumWidth() const;
    void setMinimumWidth(int minw);
    Q_SIGNAL void minimumWidthChanged();
    int minimumHeight() const;
    void setMinimumHeight(int minh);
    Q_SIGNAL void minimumHeightChanged();
    int maximumWidth() const;
    void setMaximumWidth(int maxw);
    Q_SIGNAL void maximumWidthChanged();
    int maximumHeight() const;
    void setMaximumHeight(int maxh);
    Q_SIGNAL void maximumHeightChanged();
    bool isVisible() const;
    void setVisible(bool visible);
    Q_SIGNAL void visibleChanged();
    qreal opacity() const;
    void setOpacity(qreal opacity);
    Q_SIGNAL void opacityChanged();
    QColor color() const;
    void setColor(const QColor &c);
    Q_SIGNAL void colorChanged();
    bool isActive() const;
    Q_SIGNAL void activeChanged();
    QQuickItem *activeFocusItem() const;
    Q_SIGNAL void activeFocusItemChanged();
    QWindow::Visibility visibility() const;
    void setVisibility(QWindow::Visibility newVisibility);
    Q_SIGNAL void visibilityChanged(QWindow::Visibility newVisibility);

    Q_INVOKABLE bool setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;
    Q_SIGNAL void windowPropertyChanged(const QString &name, const QVariant &value);

    Q_INVOKABLE void close();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void show();
    Q_INVOKABLE void showFullScreen();
    Q_INVOKABLE void showMinimized();
    Q_INVOKABLE void showMaximized();
    Q_INVOKABLE void showNormal();

    // pass-through signals and slots from/to the actual QQuickWindow
    Q_SIGNAL void frameSwapped();
    Q_SIGNAL void sceneGraphInitialized();
    Q_SIGNAL void sceneGraphInvalidated();
    Q_SIGNAL void beforeSynchronizing();
    Q_SIGNAL void afterSynchronizing();
    Q_SIGNAL void beforeRendering();
    Q_SIGNAL void afterRendering();
    Q_SIGNAL void afterAnimating();
    Q_SIGNAL void sceneGraphAboutToStop();
    Q_SIGNAL void beforeFrameBegin();
    Q_SIGNAL void afterFrameEnd();
    Q_SLOT void update();
    Q_SLOT void releaseResources();

    // pass-through signals and slots from/to the actual QWindow
    void requestUpdate();

    ApplicationManagerWindowImpl *implementation();

protected:
    std::unique_ptr<ApplicationManagerWindowImpl> m_impl;

private:
    static void data_append(QQmlListProperty<QObject> *property, QObject *object);
    static qsizetype data_count(QQmlListProperty<QObject> *property);
    static QObject *data_at(QQmlListProperty<QObject> *property, qsizetype index);
    static void data_clear(QQmlListProperty<QObject> *property);

    Q_DISABLE_COPY_MOVE(ApplicationManagerWindow)
};

class ApplicationManagerWindowAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QtAM::ApplicationManagerWindow *window READ window NOTIFY windowChanged FINAL)
    Q_PROPERTY(QObject *backingObject READ backingObject NOTIFY backingObjectChanged FINAL)
    Q_PROPERTY(QWindow::Visibility visibility READ visibility NOTIFY visibilityChanged FINAL)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(QQuickItem *activeFocusItem READ activeFocusItem NOTIFY activeFocusItemChanged FINAL)
    Q_PROPERTY(QQuickItem *contentItem READ contentItem NOTIFY contentItemChanged FINAL)
    Q_PROPERTY(int width READ width NOTIFY widthChanged FINAL)
    Q_PROPERTY(int height READ height NOTIFY heightChanged FINAL)

public:
    explicit ApplicationManagerWindowAttached(QObject *attachee);

    ApplicationManagerWindow *window() const;
    Q_SIGNAL void windowChanged();
    QObject *backingObject() const;
    Q_SIGNAL void backingObjectChanged();
    QWindow::Visibility visibility() const;
    Q_SIGNAL void visibilityChanged();
    bool isActive() const;
    Q_SIGNAL void activeChanged();
    QQuickItem *activeFocusItem() const;
    Q_SIGNAL void activeFocusItemChanged();
    QQuickItem *contentItem() const;
    Q_SIGNAL void contentItemChanged();
    int width() const;
    Q_SIGNAL void widthChanged();
    int height() const;
    Q_SIGNAL void heightChanged();

    QQuickItem *attachee();
    void reconnect(ApplicationManagerWindow *newWin); // callback for implementation

protected:
    std::unique_ptr<ApplicationManagerWindowAttachedImpl> m_impl;
    QPointer<ApplicationManagerWindow> m_amwindow;

private:
    Q_DISABLE_COPY_MOVE(ApplicationManagerWindowAttached)
};


QT_END_NAMESPACE_AM
