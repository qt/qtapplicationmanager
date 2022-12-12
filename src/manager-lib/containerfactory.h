// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractContainer;
class AbstractContainerManager;

class ContainerFactory : public QObject
{
    Q_OBJECT

public:
    static ContainerFactory *instance();
    ~ContainerFactory();

    QStringList containerIds() const;

    AbstractContainerManager *manager(const QString &id);
    AbstractContainer *create(const QString &id, Application *app,
                              QVector<int> &&stdioRedirections = QVector<int>(),
                              const QMap<QString, QString> &debugWrapperEnvironment = QMap<QString, QString>(),
                              const QStringList &debugWrapperCommand = QStringList());

    void setConfiguration(const QVariantMap &configuration);

    bool registerContainer(AbstractContainerManager *manager);
    bool registerContainer(AbstractContainerManager *manager, const QString &identifier);

private:
    ContainerFactory(QObject *parent = nullptr);
    ContainerFactory(const ContainerFactory &);
    ContainerFactory &operator=(const ContainerFactory &);
    static ContainerFactory *s_instance;

    QMap<QString, AbstractContainerManager *> m_containers;
};

QT_END_NAMESPACE_AM
