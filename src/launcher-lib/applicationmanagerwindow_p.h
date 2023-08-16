// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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

protected:
    void classBegin() override;

signals:
    void windowPropertyChanged(const QString &name, const QVariant &value);

private:
    ApplicationManagerWindowPrivate *d;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
