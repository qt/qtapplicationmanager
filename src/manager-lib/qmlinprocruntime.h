// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QMLINPROCRUNTIME_H
#define QMLINPROCRUNTIME_H

#include <QtAppManManager/abstractruntime.h>

#include <QtCore/QSharedPointer>

QT_FORWARD_DECLARE_CLASS(QQmlContext)

QT_BEGIN_NAMESPACE_AM

class ApplicationInterfaceImpl;
class InProcessSurfaceItem;

class QmlInProcRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit QmlInProcRuntimeManager(QObject *parent = nullptr);
    explicit QmlInProcRuntimeManager(const QString &id, QObject *parent = nullptr);

    bool inProcess() const override;

    AbstractRuntime *create(AbstractContainer *container, Application *app) override;
};


class QmlInProcRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit QmlInProcRuntime(Application *app, QmlInProcRuntimeManager *manager);
    ~QmlInProcRuntime() override;

    void openDocument(const QString &document, const QString &mimeType) override;
    qint64 applicationProcessId() const override;

    static QmlInProcRuntime *determineRuntime(QObject *object);

public Q_SLOTS:
    bool start() override;
    void stop(bool forceKill = false) override;
    void stopIfNoVisibleSurfaces();

Q_SIGNALS:
    void aboutToStop(); // used for the ApplicationInterface

private Q_SLOTS:
    void finish(int exitCode, QtAM::Am::ExitStatus status);
    void onSurfaceItemReleased(QtAM::InProcessSurfaceItem *surface);

private:
    static const char *s_runtimeKey;

    QString m_document;
    std::unique_ptr<ApplicationInterfaceImpl> m_applicationInterfaceImpl;

    bool m_stopIfNoVisibleSurfaces = false;

    void loadResources(const QStringList &resources, const QString &baseDir);
    void addPluginPaths(const QStringList &pluginPaths, const QString &baseDir);
    void addImportPaths(const QStringList &importPaths, const QString &baseDir);

    // used by QmlInProcApplicationManagerWindowImpl to register surfaceItems
    void addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface);

    bool hasVisibleSurfaces() const;

    QObject *m_rootObject = nullptr;
    QList<QSharedPointer<InProcessSurfaceItem>> m_surfaces;

    friend class QmlInProcApplicationManagerWindowImpl; // for emitting signals on behalf of this class in onComplete
    friend class QmlInProcApplicationInterfaceImpl; // for handling the quit() signal
};

QT_END_NAMESPACE_AM

#endif // QMLINPROCRUNTIME_H
