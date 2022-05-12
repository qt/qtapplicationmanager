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

#include <QMap>
#include <QObject>
#include <QVector>
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
                              const QVector<int> &stdioRedirections = QVector<int>(),
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
