// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <unistd.h>
#include <QQuickWindow>
#include <QThread>
#include <QDebug>
#include <QLoggingCategory>
#include "glitches.h"


Q_DECLARE_LOGGING_CATEGORY(appGlitches)
Q_LOGGING_CATEGORY(appGlitches, "app.glitches")

Glitches::Glitches(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *win) {
        if (win) {
            connect(win, &QQuickWindow::beforeRendering, this, [this] {
                if (m_sleepRender) {
                    qreal s = m_sleepRender;
                    m_sleepRender = 0;
                    qCInfo(appGlitches) << "Render thread: going to sleep for" << s << "seconds in render state ...";
                    QThread::msleep(s * 1000.0);
                    qCInfo(appGlitches) << "Render thread: returning from" << s << "seconds sleep in render state";
                }
            }, Qt::DirectConnection);
        }
    });
}

void Glitches::sleepGui() const
{
    qCInfo(appGlitches) << "GUI thread: going to sleep for" << m_duration << "seconds ...";
    QThread::msleep(m_duration * 1000.0);
    qCInfo(appGlitches) << "GUI thread: returning from" << m_duration << "seconds sleep";
}

void Glitches::sleepSync()
{
    m_sleepSync = m_duration;
    update();
}

void Glitches::sleepRender()
{
    m_sleepRender = m_duration;
    update();
}

QSGNode *Glitches::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (m_sleepSync) {
        qreal s = m_sleepSync;
        m_sleepSync = 0;
        qCInfo(appGlitches) << "Render thread: going to sleep in for" << s << "seconds in sync state ...";
        QThread::msleep(s * 1000.0);
        qCInfo(appGlitches) << "Render thread: returning from" << s << "seconds sleep in sync state";
    }
    return oldNode;
}
