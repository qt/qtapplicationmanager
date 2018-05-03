/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#if !defined(AM_HEADLESS)
#  include <QColor>
#  include <QQuickItem>
#  include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlComponentAttached)

QT_BEGIN_NAMESPACE_AM

class QmlInProcessRuntime;
class InProcessSurfaceItem;

class FakeApplicationManagerWindow : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QString title READ dummyGetterString WRITE dummySetterString)

    Q_PROPERTY(bool visible READ isFakeVisible WRITE setFakeVisible NOTIFY visibleChanged FINAL)

    // for API compatibility with QWaylandQuickItem - we cannot really simulate these,
    // but at least the QML code will not throw errors due to missing properties.
    Q_PROPERTY(bool paintEnabled READ dummyGetter WRITE dummySetter)
    Q_PROPERTY(bool touchEventsEnabled READ dummyGetter WRITE dummySetter)
    Q_PROPERTY(bool inputEventsEnabled READ dummyGetter WRITE dummySetter)
    Q_PROPERTY(bool focusOnClick READ dummyGetter WRITE dummySetter)

    // Hide the following properties (from QQuickIem),
    // since they are not available in multi-process mode (QWindow):
    Q_PROPERTY(QJSValue parent READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue children READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue resources READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue z READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue enabled READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue visibleChildren READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue states READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue transitions READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue state READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue childrenRect READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue anchors READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue left READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue right READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue horizontalCenter READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue top READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue bottom READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue verticalCenter READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue baseline READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue clip READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue focus READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue activeFocus READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue activeFocusOnTab READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue rotation READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue scale READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue transformOrigin READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue transformOriginPoint READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue transform READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue smooth READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue antialiasing READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue implicitWidth READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue implicitHeight READ getUndefined CONSTANT)
    Q_PROPERTY(QJSValue layer READ getUndefined CONSTANT)

public:
    explicit FakeApplicationManagerWindow(QQuickItem *parent = nullptr);
    ~FakeApplicationManagerWindow() override;

    QColor color() const;
    void setColor(const QColor &c);

    bool isFakeVisible() const;
    void setFakeVisible(bool visible);

    Q_INVOKABLE bool setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;

    void componentComplete() override;

public slots:
    void close();
    void showFullScreen();
    void showMaximized();
    void showNormal();
    //    bool close() { emit fakeCloseSignal(); return true; } // not supported right now, because it causes crashes in multiprocess mode
    //... revisit later (after andies resize-redesign) to check if close() is working for wayland

    // following QWindow slots aren't implemented yet:
    //    void hide()
    //    void lower()
    //    void raise()
    //    void setHeight(int arg)
    //    void setMaximumHeight(int h)
    //    void setMaximumWidth(int w)
    //    void setMinimumHeight(int h)
    //    void setMinimumWidth(int w)
    //    void setTitle(const QString &)
    //    void setVisible(bool visible)
    //    void setWidth(int arg)
    //    void setX(int arg)
    //    void setY(int arg)
    //    void show()
    //    void showMinimized()

    // Hide the following functions (from QQuickIem),
    // since they are not available in multi-process mode (QWindow):
    Q_INVOKABLE void grabToImage() const;
    Q_INVOKABLE void mapFromItem() const;
    Q_INVOKABLE void mapToItem() const;
    Q_INVOKABLE void mapFromGlobal() const;
    Q_INVOKABLE void mapToGlobal() const;
    Q_INVOKABLE void forceActiveFocus() const;
    Q_INVOKABLE void nextItemInFocusChain() const;
    Q_INVOKABLE void childAt() const;
#if defined(Q_CC_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
    // Although "contains" is virtual in QQuickItem with a different signature, from QML
    // the following, intentionally overloaded version without parameters will be called:
    Q_INVOKABLE void contains() const;
#if defined(Q_CC_CLANG)
#pragma clang diagnostic pop
#endif

signals:
    void fakeCloseSignal();
    void fakeFullScreenSignal();
    void fakeNoFullScreenSignal(); // TODO this should be replaced by 'normal' and 'maximized' as soon as needed
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void colorChanged();
    void visibleChanged();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void connectNotify(const QMetaMethod &signal) override;

private:
    bool dummyGetter() const { return false; }
    void dummySetter(bool) { }
    QString dummyGetterString() const { return QString(); }
    void dummySetterString(const QString&) {}

    void determineRuntime();
    void onVisibleChanged();
    QJSValue getUndefined() const;
    void referenceError(const char *symbol) const;

    InProcessSurfaceItem *m_surfaceItem = nullptr;
    QSharedPointer<QObject> m_windowProperties;
    QmlInProcessRuntime *m_runtime = nullptr;
    QColor m_color;
    bool m_fakeVisible = true;
    QVector<QQmlComponentAttached *> m_attachedCompleteHandlers;

    friend class QmlInProcessRuntime; // for setting the m_runtime member
    friend class InProcessSurfaceItem;
};

QT_END_NAMESPACE_AM

#endif // !AM_HEADLESS
