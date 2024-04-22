// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSCONTAINER_H
#define PROCESSCONTAINER_H

#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManManager/amnamespace.h>

QT_FORWARD_DECLARE_CLASS(QProcess)
QT_FORWARD_DECLARE_CLASS(QProcessEnvironment)

QT_BEGIN_NAMESPACE_AM

class MemoryWatcher;

class ProcessContainerManager : public AbstractContainerManager
{
    Q_OBJECT
public:
    explicit ProcessContainerManager(QObject *parent = nullptr);
    explicit ProcessContainerManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();
    bool supportsQuickLaunch() const override;

    AbstractContainer *create(Application *app, QVector<int> &&stdioRedirections,
                              const QMap<QString, QString> &debugWrapperEnvironment,
                              const QStringList &debugWrapperCommand) override;
};


class HostProcess : public AbstractContainerProcess
{
    Q_OBJECT

public:
    HostProcess();
    virtual ~HostProcess() override;

    virtual qint64 processId() const override;
    virtual Am::RunState state() const override;

    void setStdioRedirections(QVector<int> &&stdioRedirections);
    void setWorkingDirectory(const QString &dir);
    void setProcessEnvironment(const QProcessEnvironment &environment);

public Q_SLOTS:
    void kill() override;
    void terminate() override;

    void start(const QString &program, const QStringList &arguments);
    void setStopBeforeExec(bool stopBeforeExec);

private:
    QProcess *m_process;
    qint64 m_pid = 0;
    bool m_stopBeforeExec = false;
    QVector<int> m_stdioRedirections;
};

class ProcessContainer : public AbstractContainer
{
    Q_OBJECT

public:
    explicit ProcessContainer(ProcessContainerManager *manager, Application *app,
                              QVector<int> &&stdioRedirections,
                              const QMap<QString, QString> &debugWrapperEnvironment,
                              const QStringList &debugWrapperCommand);
    ~ProcessContainer() override;

    QString controlGroup() const override;
    bool setControlGroup(const QString &groupName) override;

    bool isReady() override;

    AbstractContainerProcess *start(const QStringList &arguments,
                                    const QMap<QString, QString> &runtimeEnvironment,
                                    const QVariantMap &amConfig) override;

private:
    QString m_currentControlGroup;
    QVector<int> m_stdioRedirections;
    QMap<QString, QString> m_debugWrapperEnvironment;
    QStringList m_debugWrapperCommand;
    MemoryWatcher *m_memWatcher = nullptr;
};

QT_END_NAMESPACE_AM

#endif // PROCESSCONTAINER_H
