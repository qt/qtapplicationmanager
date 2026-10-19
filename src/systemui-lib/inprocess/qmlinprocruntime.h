// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QMLINPROCRUNTIME_H
#define QMLINPROCRUNTIME_H

#include <memory>

#include <QtAppManSystemUI/abstractruntime.h>

#include <QtCore/QSharedPointer>
#include <QtQml/QQmlIncubator>

QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

QT_BEGIN_NAMESPACE_AM

class ApplicationInterfaceImpl;
class InProcessSurfaceItem;
class InProcessIncubator;

class Q_APPMANSYSTEMUI_EXPORT QmlInProcRuntimeManager : public AbstractRuntimeManager
{
    Q_OBJECT
public:
    explicit QmlInProcRuntimeManager(QObject *parent = nullptr);
    explicit QmlInProcRuntimeManager(const QString &id, QObject *parent = nullptr);

    bool inProcess() const override;

    AbstractRuntime *create(AbstractContainer *container, Application *app) override;
};


class Q_APPMANSYSTEMUI_EXPORT QmlInProcRuntime : public AbstractRuntime
{
    Q_OBJECT

public:
    explicit QmlInProcRuntime(Application *app, QmlInProcRuntimeManager *manager);
    ~QmlInProcRuntime() override;

    void openDocument(const QString &document, const QString &mimeType) override;
    qint64 applicationProcessId() const override;

    static QmlInProcRuntime *determineRuntime(QObject *object);

    bool start() override;
    void stop(Am::ExitStatus exitStatus) override;
    void stopIfLastWindowClosed();
    void setApplicationExtraDirs(const QMap<QString, QString> &extraPaths) override;

Q_SIGNALS:
    void aboutToStop(); // used for the ApplicationInterface

private:
    void finish(QtAM::Am::ExitStatus status);
    void finish(int exitCode, QtAM::Am::ExitStatus status);
    void onSurfaceItemReleased(QtAM::InProcessSurfaceItem *surface);
    void releaseUnannouncedSurfaces();

private:
    static const char *s_runtimeKey;

    QString m_document;
    std::unique_ptr<ApplicationInterfaceImpl> m_applicationInterfaceImpl;

    void loadResources(const QStringList &resources, const QString &baseDir);
    void addPluginPaths(const QStringList &pluginPaths, const QString &baseDir);
    void addImportPaths(const QStringList &importPaths, const QString &baseDir);

    void incubate();
    void incubationStatusChanged(QQmlIncubator::Status status);

    // used by QmlInProcApplicationManagerWindowImpl to register surfaceItems
    void addSurfaceItem(const QSharedPointer<InProcessSurfaceItem> &surface);

    QObject *m_rootObject = nullptr;
    QList<QSharedPointer<InProcessSurfaceItem>> m_surfaces;
    QQmlComponent *m_component = nullptr;
    QString m_componentErrorString;
    std::unique_ptr<InProcessIncubator> m_incubator;
    bool m_allWindowsClosed = false;
    QMap<QString, QString> m_applicationExtraDirs;

    friend class QmlInProcApplicationManagerWindowImpl; // for emitting signals on behalf of this class in onComplete
    friend class QmlInProcApplicationInterfaceImpl; // for handling the quit() signal
    friend class InProcessIncubator; // for calling incubationStatusChanged
};

QT_END_NAMESPACE_AM

#endif // QMLINPROCRUNTIME_H
