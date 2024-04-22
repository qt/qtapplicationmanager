// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef LAUNCHER_QML_P_H
#define LAUNCHER_QML_P_H

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

class ApplicationMain;
class ApplicationInterface;

class Controller : public QObject
{
    Q_OBJECT

public:
    Controller(ApplicationMain *am, bool quickLaunched);
    Controller(ApplicationMain *am, bool quickLaunched, const QPair<QString, QString> &directLoad);

public Q_SLOTS:
    void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                          const QString &mimeType, const QVariantMap &application,
                          const QVariantMap &systemProperties);

private:
    QQmlApplicationEngine m_engine;
    ApplicationInterface *m_applicationInterface = nullptr;
    QVariantMap m_configuration;
    bool m_launched = false;
    bool m_quickLaunched;
    QQuickWindow *m_window = nullptr;
    QVector<QPointer<QQuickWindow>> m_allWindows;

    void updateSlowAnimationsForWindow(QQuickWindow *window);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private Q_SLOTS:
    void updateSlowAnimations(bool isSlow);
};

QT_END_NAMESPACE_AM

#endif // LAUNCHER_QML_P_H
