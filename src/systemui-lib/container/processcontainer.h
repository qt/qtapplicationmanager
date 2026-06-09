// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSCONTAINER_H
#define PROCESSCONTAINER_H

#include <QtAppManCommon/unix-utilities.h>
#include <QtAppManSystemUI/abstractcontainer.h>
#include <QtAppManSystemUI/amnamespace.h>

QT_FORWARD_DECLARE_CLASS(QProcess)
QT_FORWARD_DECLARE_CLASS(QProcessEnvironment)

QT_BEGIN_NAMESPACE_AM

class Q_APPMANSYSTEMUI_EXPORT ProcessContainerManager : public AbstractContainerManager
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


class Q_APPMANSYSTEMUI_EXPORT HostProcess : public AbstractContainerProcess
{
    Q_OBJECT

public:
    HostProcess();
    ~HostProcess() override;

    qint64 processId() const override;
    int processFd() const override;
    Am::RunState state() const override;

    void setStdioRedirections(QVector<int> &&stdioRedirections);
    void setWorkingDirectory(const QString &dir);
    void setProcessEnvironment(const QProcessEnvironment &environment);

public Q_SLOTS:
    void stop(QtAM::Am::ExitStatus exitStatus) override;

    void start(const QString &program, const QStringList &arguments);
    void setStopBeforeExec(bool stopBeforeExec);

private:
    QProcess *m_process;
    qint64 m_pid = 0;
    Unix::Fd m_pidFd;
    bool m_stopBeforeExec = false;
    QVector<int> m_stdioRedirections;
};

class Q_APPMANSYSTEMUI_EXPORT ProcessContainer : public AbstractContainer
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
    bool hasDebugWrapper() const override;

    AbstractContainerProcess *start(const QStringList &arguments,
                                    const QMap<QString, QString> &runtimeEnvironment,
                                    const QVariantMap &amConfig) override;

private:
    QString m_currentControlGroup;
    QVector<int> m_stdioRedirections;
    QMap<QString, QString> m_debugWrapperEnvironment;
    QStringList m_debugWrapperCommand;
    static bool s_hasCGroupV2;
};

QT_END_NAMESPACE_AM

#endif // PROCESSCONTAINER_H
