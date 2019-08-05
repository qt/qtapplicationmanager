/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

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

    Q_PROPERTY(QString controlGroup READ controlGroup WRITE setControlGroup)

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
    void applicationChanged(Application *newApplication);

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
