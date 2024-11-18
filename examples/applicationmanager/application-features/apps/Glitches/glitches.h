// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef GLITCHES_H
#define GLITCHES_H

#include <QQuickItem>


class Glitches : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal duration MEMBER m_duration NOTIFY durationChanged FINAL)

public:
    explicit Glitches(QQuickItem *parent = nullptr);

    Q_INVOKABLE void sleepGui() const;
    Q_INVOKABLE void sleepSync();
    Q_INVOKABLE void sleepRender();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

Q_SIGNALS:
   void durationChanged();

private:
    qreal m_duration = 0.0;
    qreal m_sleepSync = 0.0;
    qreal m_sleepRender = 0.0;
};

#endif  // GLITCHES_H
