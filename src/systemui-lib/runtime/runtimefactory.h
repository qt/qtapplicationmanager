// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef RUNTIMEFACTORY_H
#define RUNTIMEFACTORY_H

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractRuntime;
class AbstractRuntimeManager;
class AbstractContainer;


class Q_APPMANSYSTEMUI_EXPORT RuntimeFactory : public QObject
{
    Q_OBJECT
public:
    static RuntimeFactory *instance();
    ~RuntimeFactory() override;

    QStringList runtimeIds() const;
    QString resolveRuntimeId(const QString &id) const;

    AbstractRuntimeManager *manager(const QString &id);
    AbstractRuntime *create(AbstractContainer *container, Application *app);
    AbstractRuntime *createQuickLauncher(AbstractContainer *container, const QString &id);

    void setConfiguration(const QVariantMap &configuration);
    void setSlowAnimations(bool isSlow);

    bool registerRuntime(AbstractRuntimeManager *manager);
    bool registerRuntime(AbstractRuntimeManager *manager, const QString &identifier);
    bool registerRuntimeAlias(const QString &alias, const QString &runtimeId);

private:
    RuntimeFactory(QObject *parent = nullptr);
    RuntimeFactory(const RuntimeFactory &);
    RuntimeFactory &operator=(const RuntimeFactory &);
    static RuntimeFactory *s_instance;

    QHash<QString, AbstractRuntimeManager *> m_runtimes;
    QHash<QString, QString> m_aliases;  // alias -> canonical runtime id

    // To be passed to newly created runtimes
    bool m_slowAnimations{false};
};

QT_END_NAMESPACE_AM

#endif // RUNTIMEFACTORY_H
