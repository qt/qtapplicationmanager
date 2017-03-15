/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QUrl>
#include <QStringList>
#include <QWaitCondition>
#include <QMutex>

#include <QtAppManInstaller/applicationinstaller.h>
#include <QtAppManApplication/installationreport.h>
#include <QtAppManInstaller/asynchronoustask.h>
#include <QtAppManInstaller/scopeutilities.h>

QT_BEGIN_NAMESPACE_AM

class Application;
class PackageExtractor;


class InstallationTask : public AsynchronousTask
{
    Q_OBJECT
public:
    InstallationTask(const InstallationLocation &installationLocation, const QUrl &sourceUrl,
                     QObject *parent = nullptr);
    ~InstallationTask();

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
    ApplicationInstaller *m_ai;
    const InstallationLocation &m_installationLocation;
    QUrl m_sourceUrl;
    bool m_foundInfo = false;
    bool m_foundIcon = false;
    bool m_locked = false;
    uint m_extractedFileCount = 0;
    bool m_managerApproval = false;
    QScopedPointer<Application> m_app;
    uint m_applicationUid = uint(-1);

    // changes to these 4 member variables are protected by m_mutex
    PackageExtractor *m_extractor = nullptr;
    bool m_canceled = false;
    bool m_installationAcknowledged = false;
    QWaitCondition m_installationAcknowledgeWaitCondition;

    QDir m_manifestDir;
    QDir m_applicationDir;
    QDir m_extractionDir;
    QString m_applicationImageFile;
    QString m_extractionImageFile;

    ScopedDirectoryCreator m_manifestDirPlusCreator;
    ScopedDirectoryCreator m_installationDirCreator;
    ScopedFileCreator m_imageCreator;
    ScopedDirectoryCreator m_tmpMountPointCreator;
    ScopedLoopbackCreator m_loopbackCreator;
    ScopedMounter m_imageMounter;
};

QT_END_NAMESPACE_AM
