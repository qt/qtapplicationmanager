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

#include <private/qquickwindowmodule_p.h>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QPlatformWindow)

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindowPrivate;

class ApplicationManagerWindow : public QQuickWindowQmlImpl
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.Application/ApplicationManagerWindow 2.0")

public:
    explicit ApplicationManagerWindow(QWindow *parent = nullptr);
    ~ApplicationManagerWindow();

    Q_INVOKABLE void setWindowProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant windowProperty(const QString &name) const;
    Q_INVOKABLE QVariantMap windowProperties() const;

signals:
    void windowPropertyChanged(const QString &name, const QVariant &value);

private:
    ApplicationManagerWindowPrivate *d;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
