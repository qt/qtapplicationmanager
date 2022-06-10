// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QSharedPointer>

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
