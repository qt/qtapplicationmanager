/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include "logging.h"
#include "applicationinstaller.h"
#include "applicationinstaller_p.h"
#include "applicationmanager.h"
#include "installationreport.h"
#include "applicationinfo.h"
#include "exception.h"
#include "scopeutilities.h"
#include "deinstallationtask.h"

QT_BEGIN_NAMESPACE_AM

DeinstallationTask::DeinstallationTask(ApplicationInfo *app, const InstallationLocation &installationLocation,
                                       bool forceDeinstallation, bool keepDocuments, QObject *parent)
    : AsynchronousTask(parent)
    , m_app(app)
    , m_installationLocation(installationLocation)
    , m_forceDeinstallation(forceDeinstallation)
    , m_keepDocuments(keepDocuments)
{
    m_applicationId = m_app->id(); // in base class
}

void DeinstallationTask::execute()
{
    // these have been checked in ApplicationInstaller::removePackage() already
    Q_ASSERT(m_app);
    Q_ASSERT(m_app->installationReport());
    Q_ASSERT(m_app->installationReport()->installationLocationId() == m_installationLocation.id());
    Q_ASSERT(m_installationLocation.isValid());

    bool managerApproval = false;

    try {
        // we need to call those ApplicationManager methods in the correct thread
        // this will also exclusively lock the application for us
        QMetaObject::invokeMethod(ApplicationManager::instance(),
                                  "startingApplicationRemoval",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, managerApproval),
                                  Q_ARG(QString, m_app->id()));
        if (!managerApproval)
            throw Exception("ApplicationManager rejected the removal of app %1").arg(m_app->id());


        ScopedRenamer docDirRename;
        ScopedRenamer appDirRename;
        ScopedRenamer appImageRename;
        ScopedRenamer manifestRename;

        if (!m_keepDocuments) {
            if (!docDirRename.rename(QDir(m_installationLocation.documentPath()).absoluteFilePath(m_app->id()),
                                     ScopedRenamer::NameToNameMinus)) {
                throw Exception(Error::IO, "could not rename %1 to %1-").arg(docDirRename.baseName());
            }
        }

        if (m_installationLocation.isRemovable()) {
            QString imageFile = QDir(m_installationLocation.installationPath()).absoluteFilePath(m_app->id() + qSL(".appimg"));

            if (m_installationLocation.isMounted() && QFile::exists(imageFile)) {
                // the correct medium is currently mounted
                if (!appImageRename.rename(imageFile, ScopedRenamer::NameToNameMinus)
                        && !m_forceDeinstallation) {
                    throw Exception(Error::IO, "could not rename %1 to %1-").arg(appImageRename.baseName());
                }
            } else {
                // either no medium or the wrong one are mounted at the moment
                if (!m_forceDeinstallation)
                    throw Exception(Error::MediumNotAvailable, "cannot delete application %1 without the removable medium it was installed on").arg(m_applicationId);
            }
        } else {
            if (!appDirRename.rename(QDir(m_installationLocation.installationPath()).absoluteFilePath(m_app->id()),
                                     ScopedRenamer::NameToNameMinus)) {
                throw Exception(Error::IO, "could not rename %1 to %1-").arg(appDirRename.baseName());
            }
        }

        if (!manifestRename.rename(ApplicationInstaller::instance()->manifestDirectory()->absoluteFilePath(m_app->id()),
                                   ScopedRenamer::NameToNameMinus)) {
            throw Exception(Error::IO, "could not rename %1 to %1-").arg(manifestRename.baseName());
        }

        manifestRename.take();
        docDirRename.take();
        appDirRename.take();
        appImageRename.take();

        // point of no return

        for (ScopedRenamer *toDelete : { &manifestRename, &docDirRename, &appDirRename, &appImageRename}) {
            if (toDelete->isRenamed()) {
                if (!removeRecursiveHelper(toDelete->baseName() + qL1C('-')))
                    qCCritical(LogInstaller) << "ERROR: could not remove" << (toDelete->baseName() + qL1C('-'));
            }
        }

        // we need to call those ApplicationManager methods in the correct thread
        bool finishOk = false;
        QMetaObject::invokeMethod(ApplicationManager::instance(),
                                  "finishedApplicationInstall",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, finishOk),
                                  Q_ARG(QString, m_applicationId));
        if (!finishOk)
            qCWarning(LogInstaller) << "ApplicationManager did not approve deinstallation of " << m_applicationId;

    } catch (const Exception &e) {
        // we need to call those ApplicationManager methods in the correct thread
        if (managerApproval) {
            bool cancelOk = false;
            QMetaObject::invokeMethod(ApplicationManager::instance(),
                                      "canceledApplicationInstall",
                                      Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(bool, cancelOk),
                                      Q_ARG(QString, m_applicationId));
            if (!cancelOk)
                qCWarning(LogInstaller) << "ApplicationManager could not re-enable app" << m_applicationId << "after a failed removal";
        }

        setError(e.errorCode(), e.errorString());
    }
}

QT_END_NAMESPACE_AM
