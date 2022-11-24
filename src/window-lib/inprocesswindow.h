/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QScopedPointer>

#include <QtAppManWindow/window.h>
#include <QtAppManManager/inprocesssurfaceitem.h>

QT_BEGIN_NAMESPACE_AM

class InProcessWindow : public Window
{
    Q_OBJECT

public:
    InProcessWindow(Application *app, const QSharedPointer<InProcessSurfaceItem> &surfaceItem);
    virtual ~InProcessWindow();

    bool isInProcess() const override { return true; }

    bool isPopup() const override;
    QPoint requestedPopupPosition() const override;

    bool setWindowProperty(const QString &name, const QVariant &value) override;
    QVariant windowProperty(const QString &name) const override;
    QVariantMap windowProperties() const override;

    ContentState contentState() const override { return m_contentState; }
    QSize size() const override;
    void resize(const QSize &size) override;

    void close() override;

    InProcessSurfaceItem *rootItem() const { return m_surfaceItem.data(); }
    QSharedPointer<InProcessSurfaceItem> surfaceItem() { return m_surfaceItem; }

    // The content state is wholly emulated as there's no actual surface behind this window
    // So we let it be set at will (likely by WindowManager)
    void setContentState(ContentState);

private slots:
    void onVisibleClientSideChanged();

private:
    ContentState m_contentState = SurfaceWithContent;
    QSharedPointer<InProcessSurfaceItem> m_surfaceItem;
};

QT_END_NAMESPACE_AM
