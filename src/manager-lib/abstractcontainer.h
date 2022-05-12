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
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <QtAppManCommon/global.h>
#include <QtAppManManager/amnamespace.h>

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractContainer;

class AbstractContainerManager : public QObject
{
    Q_OBJECT
public:
    AbstractContainerManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();

    QString identifier() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractContainer *create(Application *app, const QVector<int> &stdioRedirections,
                                      const QMap<QString, QString> &debugWrapperEnvironment,
                                      const QStringList &debugWrapperCommand) = 0;

    QVariantMap configuration() const;
    virtual void setConfiguration(const QVariantMap &configuration);

private:
    QString m_id;
    QVariantMap m_configuration;
};

class AbstractContainerProcess : public QObject
{
    Q_OBJECT

public:
    virtual qint64 processId() const = 0;
    virtual Am::RunState state() const = 0;

public slots:
    virtual void kill() = 0;
    virtual void terminate() = 0;

signals:
    void started();
    void errorOccured(Am::ProcessError error);
    void finished(int exitCode, Am::ExitStatus status);
    void stateChanged(Am::RunState newState);
};

class AbstractContainer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/Container 2.0 UNCREATABLE")

    Q_PROPERTY(QString controlGroup READ controlGroup WRITE setControlGroup NOTIFY controlGroupChanged)

public:
    virtual ~AbstractContainer();

    virtual QString controlGroup() const;
    virtual bool setControlGroup(const QString &groupName);

    virtual bool setProgram(const QString &program);
    virtual void setBaseDirectory(const QString &baseDirectory);

    virtual bool isReady() = 0;

    virtual QString mapContainerPathToHost(const QString &containerPath) const;
    virtual QString mapHostPathToContainer(const QString &hostPath) const;

    virtual AbstractContainerProcess *start(const QStringList &arguments,
                                            const QMap<QString, QString> &runtimeEnvironment,
                                            const QVariantMap &amConfig) = 0;

    AbstractContainerProcess *process() const;

    void setApplication(Application *app);
    Application *application() const;

signals:
    void ready();
    void memoryLowWarning();
    void memoryCriticalWarning();
    void applicationChanged(QT_PREPEND_NAMESPACE_AM(Application) *newApplication);
    void controlGroupChanged(const QString &controlGroup);

protected:
    explicit AbstractContainer(AbstractContainerManager *manager, Application *app);

    QVariantMap configuration() const;

    QString m_program;
    QString m_baseDirectory;
    AbstractContainerManager *m_manager;
    AbstractContainerProcess *m_process = nullptr;
    Application *m_app;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(AbstractContainer *))
