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

#include <QtAppManManager/abstractruntime.h>

#include <QSharedPointer>

QT_BEGIN_NAMESPACE_AM

class QmlInProcessApplicationManagerWindow;
class QmlInProcessApplicationInterface;
class InProcessSurfaceItem;

class QmlInProcessRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit QmlInProcessRuntimeManager(QObject *parent = nullptr);
    explicit QmlInProcessRuntimeManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();
    bool inProcess() const override;

    AbstractRuntime *create(AbstractContainer *container, Application *app) override;
};


class QmlInProcessRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit QmlInProcessRuntime(Application *app, QmlInProcessRuntimeManager *manager);
    ~QmlInProcessRuntime() override;

    void openDocument(const QString &document, const QString &mimeType) override;
    qint64 applicationProcessId() const override;

    static QmlInProcessRuntime *determineRuntime(QObject *object);

public slots:
    bool start() override;
    void stop(bool forceKill = false) override;
    void stopIfNoVisibleSurfaces();

signals:
    void aboutToStop(); // used for the ApplicationInterface

private slots:
    void finish(int exitCode, Am::ExitStatus status);
    void onSurfaceItemReleased(QT_PREPEND_NAMESPACE_AM(InProcessSurfaceItem) *surface);

private:
    static const char *s_runtimeKey;

    QString m_document;
    QmlInProcessApplicationInterface *m_applicationIf = nullptr;

    bool m_stopIfNoVisibleSurfaces = false;

    void loadResources(const QStringList &resources, const QString &baseDir);
    void addPluginPaths(const QStringList &pluginPaths, const QString &baseDir);
    void addImportPaths(const QStringList &importPaths, const QString &baseDir);

    // used by QmlInProcessApplicationManagerWindow to register surfaceItems
    void addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface);

    bool hasVisibleSurfaces() const;

    QObject *m_rootObject = nullptr;
    QList< QSharedPointer<InProcessSurfaceItem> > m_surfaces;

    friend class QmlInProcessApplicationManagerWindow; // for emitting signals on behalf of this class in onComplete
};

QT_END_NAMESPACE_AM
