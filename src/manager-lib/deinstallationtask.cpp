/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "logging.h"
#include "packagemanager.h"
#include "packagemanager_p.h"
#include "installationreport.h"
#include "package.h"
#include "exception.h"
#include "scopeutilities.h"
#include "deinstallationtask.h"

QT_BEGIN_NAMESPACE_AM

DeinstallationTask::DeinstallationTask(const QString &packageId, const QString &installationPath,
                                       const QString &documentPath, bool forceDeinstallation,
                                       bool keepDocuments, QObject *parent)
    : AsynchronousTask(parent)
    , m_installationPath(installationPath)
    , m_documentPath(documentPath)
    , m_keepDocuments(keepDocuments)
{
    Q_UNUSED(forceDeinstallation) // this was used in the past to deal with SD-Card problems

    m_packageId = packageId; // in base class
}

bool DeinstallationTask::cancel()
{
    if (m_canBeCanceled)
        m_canceled = true;
    return m_canceled;
}

void DeinstallationTask::execute()
{
    bool managerApproval = false;

    try {
        // we cannot rely on a check in PackageManager::removePackage()
        // things might have changed in the meantime (e.g. multiple deinstallation request)
        auto package = PackageManager::instance()->package(packageId());
        if (!package) {
            // the package has already been deinstalled - nothing more to do here
            throw Exception(Error::NotInstalled, "Cannot remove package %1 because it is not installed")
                    .arg(m_packageId);
        }

        Q_ASSERT(package->info());

        if (package->isBuiltIn() && !package->builtInHasRemovableUpdate())
            throw Exception("There is no removable update for the built-in package %1").arg(packageId());

        if (!package->info()->installationReport())
            throw Exception("Cannot remove package %1 due to missing installation report").arg(packageId());

        // we need to call those PackageManager methods in the correct thread
        // this will also exclusively lock the package for us
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &managerApproval]()
            { managerApproval = PackageManager::instance()->startingPackageRemoval(packageId()); },
            Qt::BlockingQueuedConnection);

        if (!managerApproval)
            throw Exception("PackageManager rejected the removal of package %1").arg(packageId());

        // if any of the apps in the package were running before, we now need to wait until all of
        // them have actually stopped
        while (!m_canceled && !package->areAllApplicationsStoppedDueToBlock())
            QThread::msleep(30);

        // there's a small race condition here, but not doing a planned cancellation isn't harmful
        m_canBeCanceled = false;
        if (m_canceled)
            throw Exception(Error::Canceled, "canceled");

        ScopedRenamer docDirRename;
        ScopedRenamer appDirRename;

        if (!m_keepDocuments || !m_documentPath.isEmpty()) {
            if (!docDirRename.rename(QDir(m_documentPath).absoluteFilePath(packageId()),
                                     ScopedRenamer::NameToNameMinus)) {
                throw Exception(Error::IO, "could not rename %1 to %1-").arg(docDirRename.baseName());
            }
        }

        if (!appDirRename.rename(QDir(m_installationPath).absoluteFilePath(packageId()),
                                 ScopedRenamer::NameToNameMinus)) {
            throw Exception(Error::IO, "could not rename %1 to %1-").arg(appDirRename.baseName());
        }

        docDirRename.take();
        appDirRename.take();

        // point of no return

        for (ScopedRenamer *toDelete : { &docDirRename, &appDirRename }) {
            if (toDelete->isRenamed()) {
                if (!removeRecursiveHelper(toDelete->baseName() + qL1C('-')))
                    qCCritical(LogInstaller) << "ERROR: could not remove" << (toDelete->baseName() + qL1C('-'));
            }
        }

        // we need to call those PackageManager methods in the correct thread
        bool finishOk = false;
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &finishOk]()
            { finishOk = PackageManager::instance()->finishedPackageInstall(packageId()); },
            Qt::BlockingQueuedConnection);

        if (!finishOk)
            qCWarning(LogInstaller) << "PackageManager did not approve deinstallation of " << m_packageId;

    } catch (const Exception &e) {
        // we need to call those ApplicationManager methods in the correct thread
        if (managerApproval) {
            bool cancelOk = false;
            QMetaObject::invokeMethod(PackageManager::instance(), [this, &cancelOk]()
                { cancelOk = PackageManager::instance()->canceledPackageInstall(packageId()); },
                Qt::BlockingQueuedConnection);

            if (!cancelOk)
                qCWarning(LogInstaller) << "PackageManager could not re-enable package" << m_packageId << "after a failed removal";
        }

        setError(e.errorCode(), e.errorString());
    }
}

QT_END_NAMESPACE_AM

#include "moc_deinstallationtask.cpp"
