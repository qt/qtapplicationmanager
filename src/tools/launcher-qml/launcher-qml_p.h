/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <QPointer>
#include <QQmlIncubationController>
#include <QQmlApplicationEngine>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QTimerEvent)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)

QT_BEGIN_NAMESPACE_AM

class LauncherMain;
class DBusApplicationInterface;

class Controller : public QObject
{
    Q_OBJECT

public:
    Controller(LauncherMain *launcher, bool quickLaunched);
    Controller(LauncherMain *launcher, bool quickLaunched, const QPair<QString, QString> &directLoad);

public slots:
    void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                          const QString &mimeType, const QVariantMap &application,
                          const QVariantMap &systemProperties);

private:
    QQmlApplicationEngine m_engine;
    DBusApplicationInterface *m_applicationInterface = nullptr;
    QVariantMap m_configuration;
    bool m_launched = false;
    bool m_quickLaunched;
    QQuickWindow *m_window = nullptr;
    QVector<QPointer<QQuickWindow>> m_allWindows;

    void updateSlowAnimationsForWindow(QQuickWindow *window);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void updateSlowAnimations(bool isSlow);
};

QT_END_NAMESPACE_AM
