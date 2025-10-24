// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef FRAMECONTENTTRACKER_H
#define FRAMECONTENTTRACKER_H

#include <QtCore/QObject>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QAtomicInteger>
#include <QtGui/QImage>
#include <QtAppManShared/qtappmansharedglobal.h>


QT_BEGIN_NAMESPACE_AM

class Q_APPMANSHARED_EXPORT FrameContentTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int duplicateFrames READ duplicateFrames NOTIFY updated FINAL)

    Q_PROPERTY(QObject *window READ window WRITE setWindow NOTIFY windowChanged FINAL)

    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged FINAL)

    Q_PROPERTY(QStringList roleNames READ roleNames CONSTANT FINAL)

public:
    FrameContentTracker(QObject *parent = nullptr);
    ~FrameContentTracker() override;

    QStringList roleNames() const;

    Q_INVOKABLE void update();
    Q_SIGNAL void updated();

    QObject *window() const;
    void setWindow(QObject *window);
    Q_SIGNAL void windowChanged();

    bool running() const;
    void setRunning(bool running);
    Q_SIGNAL void runningChanged();

    int interval() const;
    void setInterval(int interval);
    Q_SIGNAL void intervalChanged();

    int duplicateFrames() const;

private:
    QPointer<QObject> m_window;

    QAtomicInteger<int> m_sameFrameCounter = -1;
    QImage m_lastFrame;

    int m_duplicateFrames = -1;
    QTimer m_updateTimer;

    QMetaObject::Connection m_frameAfterRenderingConnection;
};

QT_END_NAMESPACE_AM

#endif // FRAMECONTENTTRACKER_H
