// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#if 0
#pragma qt_sync_skip_header_check
#endif

#include <QColor>
#include <QPointer>
#include <QtQuick/private/qquickfocusscope_p.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindow;

/*
 *  Item exposed to the System UI
 */
class InProcessSurfaceItem : public QQuickFocusScope
{
    Q_OBJECT
public:
    InProcessSurfaceItem(QQuickItem *parent = nullptr);
    ~InProcessSurfaceItem() override;

    bool setWindowProperty(const QString &name, const QVariant &value);
    QVariant windowProperty(const QString &name) const;
    QObject &windowProperties() { return m_windowProperties; }
    QVariantMap windowPropertiesAsVariantMap() const;

    // Whether the surface is visible on the client (application) side.
    // The when application hides its surface, System UI should, in an animated way,
    // remove it from user's view.
    // That's why a client shouldn't have control over the actual visible property
    // of its surface item.
    void setVisibleClientSide(bool);
    bool visibleClientSide() const;

    QColor color() const;
    void setColor(const QColor &c);

    void close();

    bool focusOnClick() const;
    void setFocusOnClick(bool newFocusOnClick);
    Q_SIGNAL void focusOnClickChanged();

    ApplicationManagerWindow *applicationManagerWindow();
    void setApplicationManagerWindow(ApplicationManagerWindow *win);

signals:
    void visibleClientSideChanged();
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void closeRequested();

    // Emitted when WindowManager is no longer tracking this surface item
    // (ie, the Window that was tracking this surface item has been destroyed)
    void released();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void touchEvent(QTouchEvent *e) override;
    bool childMouseEventFilter(QQuickItem *item, QEvent *e) override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    void delayedForceActiveFocus();

    QObject m_windowProperties;
    bool m_visibleClientSide = true;
    bool m_focusOnClick = false;
    QColor m_color;
    QPointer<ApplicationManagerWindow> m_amWindow;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QtAM::InProcessSurfaceItem*)
Q_DECLARE_METATYPE(QSharedPointer<QtAM::InProcessSurfaceItem>)
