// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QUrl>
#include <QStringList>
#include <QWaitCondition>
#include <QMutex>

#include <QtAppManApplication/installationreport.h>
#include <QtAppManManager/asynchronoustask.h>
#include <QtAppManManager/scopeutilities.h>

#include <memory>

QT_BEGIN_NAMESPACE_AM

class Package;
class PackageInfo;
class PackageManager;
class PackageExtractor;


class InstallationTask : public AsynchronousTask
{
    Q_OBJECT
public:
    InstallationTask(const QString &installationPath, const QString &documentPath,
                     const QUrl &sourceUrl, QObject *parent = nullptr);
    ~InstallationTask() override;

    void acknowledge();
    bool cancel() override;

signals:
    void finishedPackageExtraction();

protected:
    void execute() override;

private:
    void startInstallation() Q_DECL_NOEXCEPT_EXPR(false);
    void finishInstallation() Q_DECL_NOEXCEPT_EXPR(false);
    void checkExtractedFile(const QString &file) Q_DECL_NOEXCEPT_EXPR(false);

private:
    PackageManager *m_pm;
    QString m_installationPath;
    QString m_documentPath;
    QUrl m_sourceUrl;
    bool m_foundInfo = false;
    bool m_foundIcon = false;
    QString m_iconFileName;
    uint m_extractedFileCount = 0;
    bool m_managerApproval = false;
    std::unique_ptr<PackageInfo> m_package;
    uint m_applicationUid = uint(-1);
    QScopedPointer<Package> m_tempPackageForAcknowledge;

    // changes to these 4 member variables are protected by m_mutex
    PackageExtractor *m_extractor = nullptr;
    bool m_canceled = false;
    bool m_installationAcknowledged = false;
    QWaitCondition m_installationAcknowledgeWaitCondition;

    static QMutex s_serializeFinishInstallation;

    QDir m_applicationDir;
    QDir m_extractionDir;

    ScopedDirectoryCreator m_installationDirCreator;
};

QT_END_NAMESPACE_AM
