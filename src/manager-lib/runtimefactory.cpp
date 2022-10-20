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

#include <QScopedPointer>
#include <QCoreApplication>

#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "runtimefactory.h"

QT_BEGIN_NAMESPACE_AM

RuntimeFactory *RuntimeFactory::s_instance = nullptr;

RuntimeFactory *RuntimeFactory::instance()
{
    if (!s_instance)
        s_instance = new RuntimeFactory(QCoreApplication::instance());
    return s_instance;
}

RuntimeFactory::RuntimeFactory(QObject *parent)
    : QObject(parent)
{ }

RuntimeFactory::~RuntimeFactory()
{
    qDeleteAll(m_runtimes);
    s_instance = nullptr;
}

QStringList RuntimeFactory::runtimeIds() const
{
    return m_runtimes.keys();
}

AbstractRuntimeManager *RuntimeFactory::manager(const QString &id)
{
    if (id.isEmpty())
        return nullptr;
    AbstractRuntimeManager *arm = m_runtimes.value(id);
    if (!arm)
        arm = m_runtimes.value(id + qSL("-inprocess"));
    return arm;
}

AbstractRuntime *RuntimeFactory::create(AbstractContainer *container, Application *app)
{
    QScopedPointer<AbstractContainer> ac(container);

    if (!app || app->runtimeName().isEmpty() || app->currentRuntime())
        return nullptr;

    AbstractRuntimeManager *arm = manager(app->runtimeName());
    if (!arm)
        return nullptr;

    AbstractRuntime *art = arm->create(ac.take(), app);

    if (art) {
        art->setSlowAnimations(m_slowAnimations);
        app->setCurrentRuntime(art);
    }
    return art;
}

AbstractRuntime *RuntimeFactory::createQuickLauncher(AbstractContainer *container, const QString &id)
{
    QScopedPointer<AbstractContainer> ac(container);

    AbstractRuntimeManager *arm = manager(id);
    if (!arm)
        return nullptr;

    auto runtime = arm->create(ac.take(), nullptr);
    if (runtime)
        runtime->setSlowAnimations(m_slowAnimations);
    return runtime;
}

void RuntimeFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it)
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
}

void RuntimeFactory::setSystemProperties(const QVariantMap &thirdParty, const QVariantMap &builtIn)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it)
        it.value()->setSystemProperties(thirdParty, builtIn);
}

void RuntimeFactory::setSlowAnimations(bool value)
{
    m_slowAnimations = value;
}

void RuntimeFactory::setSystemOpenGLConfiguration(const QVariantMap &openGLConfiguration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it)
        it.value()->setSystemOpenGLConfiguration(openGLConfiguration);
}

void RuntimeFactory::setIconTheme(const QStringList &themeSearchPaths, const QString &themeName)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it)
        it.value()->setIconTheme(themeSearchPaths, themeName);
}

bool RuntimeFactory::registerRuntime(AbstractRuntimeManager *manager)
{
    return registerRuntime(manager, manager->identifier());
}

bool RuntimeFactory::registerRuntime(AbstractRuntimeManager *manager, const QString &identifier)
{
    if (!manager || identifier.isEmpty() || m_runtimes.contains(identifier))
        return false;
    m_runtimes.insert(identifier, manager);
    manager->setParent(this);
    static bool once = false;
    if (!once) {
        qCDebug(LogSystem) << "Registering runtimes:";
        once = true;
    }
    qCDebug(LogSystem).noquote() << " *" << identifier << (manager->supportsQuickLaunch() ? "[quicklaunch supported]" : "");
    return true;
}

QT_END_NAMESPACE_AM

#include "moc_runtimefactory.cpp"
