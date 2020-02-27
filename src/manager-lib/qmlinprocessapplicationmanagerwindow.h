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

#if !defined(AM_HEADLESS)
#  include <QSharedPointer>
#  include <QQmlParserStatus>
#  include <QColor>
#  include <QQuickItem>
#  include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlComponentAttached)

QT_BEGIN_NAMESPACE_AM

class QmlInProcessRuntime;
class InProcessSurfaceItem;

class QmlInProcessApplicationManagerWindow : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem CONSTANT)

    // QWindow properties
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    //Q_PROPERTY(Qt::WindowModality modality READ modality WRITE setModality NOTIFY modalityChanged)
    //Q_PROPERTY(Qt::WindowFlags flags READ flags WRITE setFlags)
    Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
    Q_PROPERTY(int minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged)
    Q_PROPERTY(int minimumHeight READ minimumHeight WRITE setMinimumHeight NOTIFY minimumHeightChanged)
    Q_PROPERTY(int maximumWidth READ maximumWidth WRITE setMaximumWidth NOTIFY maximumWidthChanged)
    Q_PROPERTY(int maximumHeight READ maximumHeight WRITE setMaximumHeight NOTIFY maximumHeightChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    //Q_PROPERTY(Visibility visibility READ visibility WRITE setVisibility NOTIFY visibilityChanged)
    //Q_PROPERTY(Qt::ScreenOrientation contentOrientation READ contentOrientation WRITE reportContentOrientationChange NOTIFY contentOrientationChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)

    Q_PROPERTY(QQmlListProperty<QObject> data READ data)

    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QmlInProcessApplicationManagerWindow(QObject *parent = nullptr);
    ~QmlInProcessApplicationManagerWindow() override;

    QColor color() const;
    void setColor(const QColor &c);

    QQmlListProperty<QObject> data();

    Q_INVOKABLE bool setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;

    QQuickItem *contentItem();

    // From QQmlParserStatus
    void classBegin() override {}
    void componentComplete() override;

    static void data_append(QQmlListProperty<QObject> *property, QObject *value);
    static int data_count(QQmlListProperty<QObject> *property);
    static QObject *data_at(QQmlListProperty<QObject> *property, int index);
    static void data_clear(QQmlListProperty<QObject> *property);

    // Getters and setters for QWindow properties

    QString title() const { return m_title; }
    void setTitle(const QString &);

    int x() const { return m_x; }
    void setX(int);

    int y() const { return m_y; }
    void setY(int);

    int width() const;
    void setWidth(int);

    int height() const;
    void setHeight(int);

    int minimumWidth() const { return m_minimumWidth; }
    void setMinimumWidth(int);

    int minimumHeight() const { return m_minimumHeight; }
    void setMinimumHeight(int);

    int maximumWidth() const { return m_maximumWidth; }
    void setMaximumWidth(int);

    int maximumHeight() const { return m_maximumHeight; }
    void setMaximumHeight(int);

    bool isVisible() const;
    void setVisible(bool visible);

    bool isActive() const { return m_active; }

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal);

public slots:
    void close();
    void showFullScreen();
    void showMaximized();
    void showNormal();

    // following QWindow slots aren't implemented yet:
    //    void hide()
    //    void lower()
    //    void raise()
    //    void show()
    //    void showMinimized()

signals:
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void colorChanged();

    // signals for QWindow properties
    void titleChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void minimumWidthChanged();
    void minimumHeightChanged();
    void maximumWidthChanged();
    void maximumHeightChanged();
    void visibleChanged();
    void activeChanged();
    void opacityChanged();

private slots:
    void notifyRuntimeAboutSurface();

private:
    void determineRuntime();
    void findParentWindow(QObject *object = nullptr);
    void setParentWindow(QmlInProcessApplicationManagerWindow *appWindow);

    static QVector<QmlInProcessApplicationManagerWindow *> s_inCompleteWindows;

    QSharedPointer<InProcessSurfaceItem> m_surfaceItem;
    QmlInProcessRuntime *m_runtime = nullptr;
    QVector<QQmlComponentAttached *> m_attachedCompleteHandlers;
    QmlInProcessApplicationManagerWindow *m_parentWindow = nullptr;

    // QWindow properties
    QString m_title;
    int m_x;
    int m_y;
    int m_minimumWidth;
    int m_minimumHeight;
    int m_maximumWidth;
    int m_maximumHeight;
    bool m_active;
    qreal m_opacity;

    friend class QmlInProcessRuntime; // for setting the m_runtime member
};

QT_END_NAMESPACE_AM

#endif // !AM_HEADLESS
