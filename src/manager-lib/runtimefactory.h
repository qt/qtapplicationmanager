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

#include <QHash>
#include <QObject>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractRuntime;
class AbstractRuntimeManager;
class AbstractContainer;


class RuntimeFactory : public QObject
{
    Q_OBJECT
public:
    static RuntimeFactory *instance();
    ~RuntimeFactory();

    QStringList runtimeIds() const;

    AbstractRuntimeManager *manager(const QString &id);
    AbstractRuntime *create(AbstractContainer *container, Application *app);
    AbstractRuntime *createQuickLauncher(AbstractContainer *container, const QString &id);

    void setConfiguration(const QVariantMap &configuration);
    void setSystemProperties(const QVariantMap &thirdParty, const QVariantMap &builtIn);
    void setSlowAnimations(bool isSlow);
    void setSystemOpenGLConfiguration(const QVariantMap &openGLConfiguration);
    void setIconTheme(const QStringList &themeSearchPaths, const QString &themeName);

    bool registerRuntime(AbstractRuntimeManager *manager);
    bool registerRuntime(AbstractRuntimeManager *manager, const QString &identifier);

private:
    RuntimeFactory(QObject *parent = nullptr);
    RuntimeFactory(const RuntimeFactory &);
    RuntimeFactory &operator=(const RuntimeFactory &);
    static RuntimeFactory *s_instance;

    QHash<QString, AbstractRuntimeManager *> m_runtimes;

    // To be passed to newly created runtimes
    bool m_slowAnimations{false};
};

QT_END_NAMESPACE_AM
