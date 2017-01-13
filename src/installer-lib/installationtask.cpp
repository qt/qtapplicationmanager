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

#include "applicationinstaller_p.h"
#include "application.h"
#include "packageextractor.h"
#include "yamlapplicationscanner.h"
#include "exception.h"
#include "applicationmanager.h"
#include "sudo.h"
#include "utilities.h"
#include "signature.h"
#include "sudo.h"
#include "installationtask.h"

/*
  Overview of what happens on an installation of an app with <id> to <location>:

  Step 1 -- startInstallation()
  =============================

  delete <manifestdir>/<id>+
  delete <manifestdir>/<id>-

  if (removable)
      delete <location>/<id>.appimg+
  else
      delete <location>/<id>+

  create <manifestdir>/<id>+

  if (removable)
      create ext2 image at <location>/<id>/.appimg+
      create loopback device on image file
      mkfs.ext2 on loopback device
      mount loopback to <imagemountdir>/<id>+
      set <extractiondir> to <imagemountdir>/<id>+
  else
      create dir <location>/<id>+
      set <extractiondir> to <location>/<id>+


  Step 2 -- unpack files
  ======================

  PackageExtractor does its job


  Step 3 -- finishInstallation()
  ================================

  if (removable)
      if (exists <location>/<id>.appimg)
          set <isupdate> to <true>
  else
      if (exists <location>/<id>)
          set <isupdate> to <true>

  create installation report at <manifestdir>/installation-report.yaml

  if (not <isupdate>)
      create document directory

  if (optional uid separation)
      chown/chmod recursively in <extractionDir> and document directory

  copy info.yaml and icon.png from <extractiondir> to <manifestdir>/<id>+

  if (removable)
      unmount loopback, destroy loopback, remove mount-point


  Step 3.1 -- final rename in finishInstallation()
  ==================================================

  rename <manifestdir>/<id> to <manifestdir>/<id>-
  rename <manifestdir>/<id>+ to <manifestdir>/<id>

  if (removable)
      rename <location>/<id>.appimg+ to <location>/<id>.appimg
  else
     if (<isupdate>)
         rename <location>/<id> to <location>/<id>-
     rename <location>/<id>+ to <location>/<id>
*/

QT_BEGIN_NAMESPACE_AM

InstallationTask::InstallationTask(const InstallationLocation &installationLocation, const QUrl &sourceUrl, QObject *parent)
    : AsynchronousTask(parent)
    , m_ai(ApplicationInstaller::instance())
    , m_installationLocation(installationLocation)
    , m_sourceUrl(sourceUrl)
{ }

InstallationTask::~InstallationTask()
{ }

bool InstallationTask::cancel()
{
    QMutexLocker locker(&m_mutex);

    // we cannot cancel anymore after finishInstallation() has been called
    if (m_installationAcknowledged)
        return false;

    m_canceled = true;
    if (m_extractor)
        m_extractor->cancel();
    m_installationAcknowledgeWaitCondition.wakeAll();
    return true;
}

void InstallationTask::acknowledge()
{
    QMutexLocker locker(&m_mutex);

    if (m_canceled)
        return;

    m_installationAcknowledged = true;
    m_installationAcknowledgeWaitCondition.wakeAll();
}

void InstallationTask::execute()
{
    try {
        if (!m_installationLocation.isValid())
            throw Exception(Error::System, "invalid installation location");

        TemporaryDir extractionDir;
        if (!extractionDir.isValid())
            throw Exception(Error::System, "could not create a temporary extraction directory");

        // protect m_canceled and changes to m_extractor
        QMutexLocker locker(&m_mutex);
        if (m_canceled)
            throw Exception(Error::Canceled, "canceled");

        m_extractor = new PackageExtractor(m_sourceUrl, QDir(extractionDir.path()));
        locker.unlock();

        connect(m_extractor, &PackageExtractor::progress, this, &AsynchronousTask::progress);

        m_extractor->setFileExtractedCallback(std::bind(&InstallationTask::checkExtractedFile,
                                                        this, std::placeholders::_1));

        if (!m_extractor->extract())
            throw Exception(m_extractor->errorCode(), m_extractor->errorString());

        if (!m_foundInfo || !m_foundIcon)
            throw Exception(Error::Package, "package did not contain a valid info.json and icon.png file");

        QList<QByteArray> chainOfTrust = m_ai->caCertificates();

        if (ApplicationManager::instance()->securityChecksEnabled()) {
            if (!m_extractor->installationReport().storeSignature().isEmpty()) {
                // normal package from the store
                if (!Signature(m_extractor->installationReport().digest()).verify(m_extractor->installationReport().storeSignature(), chainOfTrust))
                    throw Exception(Error::Package, "could not verify the package's store signature");

            } else if (!m_extractor->installationReport().developerSignature().isEmpty()) {
                // developer package - needs a device in dev mode
                if (!m_ai->developmentMode())
                    throw Exception(Error::Package, "cannot install development packages on consumer devices");

                if (!Signature(m_extractor->installationReport().digest()).verify(m_extractor->installationReport().developerSignature(), chainOfTrust))
                    throw Exception(Error::Package, "could not verify the package's developer signature");

            } else {
                if (!m_ai->allowInstallationOfUnsignedPackages())
                    throw Exception(Error::Package, "cannot install unsigned packages");
            }
        }

        emit finishedPackageExtraction();
        setState(AwaitingAcknowledge);

        // now wait in a wait-condition until we get an acknowledge or we get canceled
        locker.relock();
        while (!m_canceled && !m_installationAcknowledged)
            m_installationAcknowledgeWaitCondition.wait(&m_mutex);

        // this is the last cancellation point
        if (m_canceled)
            throw Exception(Error::Canceled, "canceled");
        locker.unlock();

        setState(Installing);

        // if we would allow parallel installations - this would be the place to serialize them

        finishInstallation();

        // At this point, the installation is done, so we cannot throw anymore.

        // we need to call those ApplicationManager methods in the correct thread
        bool finishOk = false;
        QMetaObject::invokeMethod(ApplicationManager::instance(),
                                  "finishedApplicationInstall", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, finishOk),
                                  Q_ARG(QString, m_applicationId));
        if (!finishOk)
            qCWarning(LogInstaller) << "ApplicationManager rejected the installation of " << m_applicationId;

    } catch (const Exception &e) {
        setError(e.errorCode(), e.errorString());

        if (m_managerApproval) {
            // we need to call those ApplicationManager methods in the correct thread
            bool cancelOk = false;
            QMetaObject::invokeMethod(ApplicationManager::instance(),
                                      "canceledApplicationInstall",
                                      Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(bool, cancelOk),
                                      Q_ARG(QString, m_applicationId));
            if (!cancelOk)
                qCWarning(LogInstaller) << "ApplicationManager could not remove app" << m_applicationId << "after a failed installation";
        }
    }


    {
        QMutexLocker locker(&m_mutex);
        delete m_extractor;
        m_extractor = 0;
    }
}


void InstallationTask::checkExtractedFile(const QString &file) throw(Exception)
{
    if (++m_extractedFileCount > 2)
        throw Exception(Error::Package, "could not find info.yaml and icon.png at the beginning of the package");

    if (file == qL1S("info.yaml")) {
        if (m_foundInfo)
            throw Exception(Error::Package, "found multiple info.yaml files in the package");

        YamlApplicationScanner yas;
        m_app = yas.scan(m_extractor->destinationDirectory().absoluteFilePath(file));
        if (m_app->id() != m_extractor->installationReport().applicationId())
            throw Exception(Error::Package, "the application identifiers in --PACKAGE-HEADER--' and info.yaml do not match");

        InstallationLocation existingLocation = m_ai->installationLocationFromApplication(m_app->id());

        if (existingLocation.isValid() && (existingLocation != m_installationLocation)) {
            throw Exception(Error::Package, "the application %1 cannot be installed to %2, since it is already installed to %3")
                .arg(m_app->id(), m_installationLocation.id(), existingLocation.id());
        }

        m_app->m_builtIn = false;
        m_applicationId = m_app->id();

        m_foundInfo = true;
    } else if (file == qL1S("icon.png")) {
        if (m_foundIcon)
            throw Exception(Error::Package, "found multiple icon.png files in the package");

        QFile icon(m_extractor->destinationDirectory().absoluteFilePath(file));
        if (icon.size() > 256*1024)
            throw Exception(Error::Package, "the size of icon.png is too large (max. 256KB)");

        m_foundIcon = true;
    }

    if (m_foundIcon && m_foundInfo) {
        qCDebug(LogInstaller) << "emit requestingInstallationAcknowledge" << id() << "<app>";
        emit m_ai->taskRequestingInstallationAcknowledge(id(), m_app->toVariantMap());

        QDir oldDestinationDirectory = m_extractor->destinationDirectory();

        startInstallation();

        QFile::copy(oldDestinationDirectory.filePath(qSL("info.yaml")), m_extractionDir.filePath(qSL("info.yaml")));
        QFile::copy(oldDestinationDirectory.filePath(qSL("icon.png")), m_extractionDir.filePath(qSL("icon.png")));

        {
            QMutexLocker locker(&m_mutex);
            m_extractor->setDestinationDirectory(m_extractionDir);

            QString path = m_extractionDir.absolutePath();
            path.chop(1); // remove the '+'
            m_app->setBaseDir(path); //TODO: this is not correct for Images!!!!
        }
        // we need to call those ApplicationManager methods in the correct thread
        // this will also exclusively lock the application for us
        // m_app ownership is transferred to the ApplicationManager
        QMetaObject::invokeMethod(ApplicationManager::instance(),
                                  "startingApplicationInstallation",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, m_managerApproval),
                                  // ugly, but Q_ARG chokes on QT_PREPEND_NAMESPACE_AM...
                                  QArgument<QT_PREPEND_NAMESPACE_AM(Application *)>(QT_STRINGIFY(QT_PREPEND_NAMESPACE_AM(Application *)), m_app));
        if (!m_managerApproval)
            throw Exception(Error::System, "Application Manager declined the installation of %1").arg(m_app->id());

        // now that the Manager knows about the app object, we can try to find a free uid
        m_app->m_uid = m_ai->findUnusedUserId();

        // we're not interested in any other files from here on...
        m_extractor->setFileExtractedCallback(nullptr);
    }
}

void InstallationTask::startInstallation() throw (Exception)
{
    // 1. delete $manifestDir+ and $manifestDir-
    m_manifestDir = m_ai->manifestDirectory().absoluteFilePath(m_applicationId);
    removeRecursiveHelper(m_manifestDir.absolutePath() + qL1C('+'));
    removeRecursiveHelper(m_manifestDir.absolutePath() + qL1C('-'));

    // 2. delete old, partial installation

    QDir installationDir = QString(m_installationLocation.installationPath() + qL1C('/'));
    QString installationTarget;
    if (m_installationLocation.isRemovable()) {
        installationTarget = m_applicationId + qSL(".appimg+");

        if (!m_installationLocation.isMounted()) // make sure the device is mounted
            throw Exception(Error::MediumNotAvailable, "installation medium %1 is not mounted").arg(m_installationLocation.id());

        if (m_ai->isPackageActivated(m_applicationId))
            throw Exception(Error::System, "existing application image %1 is still mounted").arg(installationTarget);
    } else {
        installationTarget = m_applicationId + qL1C('+');
    }
    if (installationDir.exists(installationTarget)) {
        if (!removeRecursiveHelper(installationDir.absoluteFilePath(installationTarget)))
            throw Exception(Error::System, "could not remove old, partial installation %1/%2").arg(installationDir).arg(installationTarget);
    }

    // 3. create $manifestDir+
    if (!m_manifestDirPlusCreator.create(m_manifestDir.absolutePath() + qL1C('+')))
        throw Exception(Error::System, "could not create manifest sub-directory %1+").arg(m_manifestDir);

    // 4. create new installation
    if (m_installationLocation.isRemovable()) {
        if (!m_installationDirCreator.create(installationDir.absolutePath()))
            throw Exception(Error::System, "could not create application image base directory %1").arg(installationDir);

        quint64 neededSize = qMax(m_extractor->installationReport().diskSpaceUsed(), quint64(70 * 1024));

        quint64 availableSize = 0;
        if (!diskUsage(installationDir.absolutePath(), 0, &availableSize) || availableSize < neededSize) {
            throw Exception(Error::StorageSpace, "not enough storage space left on %1: %2 MB available, but %3 MB needed")
                    .arg(m_installationLocation.id())
                    .arg(double(availableSize) / (1024 * 1024), 0, 'f', 2)
                    .arg(double(neededSize) / (1024 * 1024), 0, 'f', 2);
        }

        m_extractionImageFile = installationDir.absoluteFilePath(installationTarget);
        m_applicationImageFile = installationDir.absoluteFilePath(m_applicationId + qSL(".appimg"));

        QFile image(m_extractionImageFile);
        if (image.exists())
            throw Exception(Error::IO, "image file %1 already exists").arg(m_extractionImageFile);
        if (!image.open(QFile::WriteOnly | QFile::Truncate))
            throw Exception(image, "could not open image file for writing");
        if (!image.resize(neededSize)) {
            image.remove();
            throw Exception(image, "could not resize image file to %1 bytes").arg(neededSize);
        }
        m_imageCreator.create(m_extractionImageFile, false /*do not remove existing*/);

        QDir tmpMountPoint(m_ai->applicationImageMountDirectory());
        tmpMountPoint = tmpMountPoint.absoluteFilePath(m_applicationId + qL1C('+'));
        if (!m_tmpMountPointCreator.create(tmpMountPoint.absolutePath()))
            throw Exception(Error::System, "could not create temporary mountpoint %1").arg(tmpMountPoint);

        if (!m_loopbackCreator.create(m_extractionImageFile))
            throw Exception(Error::System, "could not create loopback device for %1: %2").arg(m_extractionImageFile, m_loopbackCreator.errorString());

        if (!SudoClient::instance()->mkfs(m_loopbackCreator.device()))
            throw Exception(Error::System, "could not create filesystem on device %1: %2").arg(m_loopbackCreator.device(), SudoClient::instance()->lastError());

        if (!m_imageMounter.mount(m_loopbackCreator.device(), tmpMountPoint.absolutePath(), false /*ro*/))
            throw Exception(Error::System, "could not mount device %1 on %2: %3").arg(m_loopbackCreator.device(), tmpMountPoint.absolutePath(), m_imageMounter.errorString());

        if (!m_extractionDir.cd(tmpMountPoint.absolutePath()))
            throw Exception(Error::System, "could not cd into temporary mountpoint %1").arg(tmpMountPoint);
    } else {
        if (!m_installationDirCreator.create(installationDir.absoluteFilePath(installationTarget)))
            throw Exception(Error::System, "could not create installation directory %1/%2").arg(installationDir).arg(installationTarget);
        m_extractionDir = installationDir;
        if (!m_extractionDir.cd(installationTarget))
            throw Exception(Error::System, "could not cd into installation directory %1/%2").arg(installationDir).arg(installationTarget);
        m_applicationDir = installationDir.absoluteFilePath(m_applicationId);
    }
}

void InstallationTask::finishInstallation() throw (Exception)
{
    QDir documentDirectory(m_installationLocation.documentPath());
    ScopedDirectoryCreator documentDirCreator;

    enum { Installation, Update } mode = Installation;
    enum { IntoFileSystem, IntoImage } destination = IntoFileSystem;

    if (m_installationLocation.isRemovable()) {
        destination = IntoImage;

        if (!m_installationLocation.isMounted())
            throw Exception(Error::MediumNotAvailable, "removable medium %1 is not mounted").arg(m_installationLocation.id());

        if (!QFile::exists(m_extractionImageFile))
            throw Exception(Error::System, "removable medium %1 was removed during the installation").arg(m_installationLocation.id());

        if ((destination == IntoImage) && QFile::exists(m_applicationImageFile))
            mode = Update;
    } else {
        destination = IntoFileSystem;

        if ((destination == IntoFileSystem) && m_applicationDir.exists())
            mode = Update;
    }

    // create the installation report
    InstallationReport report = m_extractor->installationReport();
    report.setInstallationLocationId(m_installationLocation.id());

    //TODO: this is not really thread-safe - find a better way
    m_app->setInstallationReport(new InstallationReport(report));

    QFile reportFile(m_manifestDirPlusCreator.dir().absoluteFilePath(qSL("installation-report.yaml")));
    if (!reportFile.open(QFile::WriteOnly) || !report.serialize(&reportFile))
        throw Exception(reportFile, "could not write the installation report");
    reportFile.close();

    // create the document directories when installing (not needed on updates)
    if (mode == Installation) {
        // this package may have been installed earlier and the document directory may not have been removed
        if (!documentDirectory.cd(m_applicationId)) {
            if (!documentDirCreator.create(documentDirectory.absoluteFilePath(m_applicationId)))
                throw Exception(Error::IO, "could not create the document directory %1").arg(documentDirectory.filePath(m_applicationId));
        }
    }
#ifdef Q_OS_UNIX
    // update the owner, group and permission bits on both the installation and document directories
    SudoClient *root = SudoClient::instance();

    if (m_ai->isApplicationUserIdSeparationEnabled() && root) {
        uid_t uid = m_app->uid();
        gid_t gid = m_ai->commonApplicationGroupId();

        if (!root->setOwnerAndPermissionsRecursive(documentDirectory.filePath(m_applicationId), uid, gid, 02700)) {
            throw Exception(Error::IO, "could not recursively change the owner to %1:%2 and the permission bits to %3 in %4")
                    .arg(uid).arg(gid).arg(02700, 0, 8).arg(documentDirectory.filePath(m_applicationId));
        }

        if (!root->setOwnerAndPermissionsRecursive(m_extractionDir.path(), uid, gid, 0440)) {
            throw Exception(Error::IO, "could not recursively change the owner to %1:%2 and the permission bits to %3 in %4")
                    .arg(uid).arg(gid).arg(0440, 0, 8).arg(m_extractionDir.absolutePath());
        }
    }
#endif

    // copy meta-data to manifest directory
    for (const QString &file : { qSL("info.yaml"), qSL("icon.png") })
        if (!QFile::copy(m_extractionDir.absoluteFilePath(file), m_manifestDirPlusCreator.dir().absoluteFilePath(file))) {
            throw Exception(Error::IO, "could not copy %1 from the application directory to the manifest directory").arg(file);
    }
    // in case we need persistent data in addition to info.yaml and icon.png,
    // we could copy these out of the image right now...

    // removable special handling
    if (destination == IntoImage) {
        //TODO: if any of these fails, we're screwed and the only way to get back to a
        //      working state, would be a reboot
        m_imageMounter.unmount();
        m_loopbackCreator.destroy();
        m_tmpMountPointCreator.destroy();
    }

    // final rename

    // POSIX cannot atomically rename directories, if the destination directory exists
    // and is non-empty. We need to do a double-rename in this case, which might fail!
    // The image is a file, so this limitation does not apply!

    ScopedRenamer renameManifest;
    ScopedRenamer renameApplication;

    if (!renameManifest.rename(m_manifestDir, (mode == Update ? ScopedRenamer::NameToNameMinus | ScopedRenamer::NamePlusToName : ScopedRenamer::NamePlusToName)))
        throw Exception(Error::IO, "could not rename manifest directory %1+ to %1 (including a backup to %1-)").arg(m_manifestDir);

    if (destination == IntoFileSystem) {
        if (mode == Update) {
            if (!renameApplication.rename(m_applicationDir, ScopedRenamer::NamePlusToName | ScopedRenamer::NameToNameMinus))
                throw Exception(Error::IO, "could not rename application directory %1+ to %1 (including a backup to %1-)").arg(m_applicationDir);
        } else {
            if (!renameApplication.rename(m_applicationDir, ScopedRenamer::NamePlusToName))
                throw Exception(Error::IO, "could not rename application directory %1+ to %1").arg(m_applicationDir);
        }
    } else if (destination == IntoImage) {
        if (!renameApplication.rename(m_applicationImageFile, ScopedRenamer::NamePlusToName))
            throw Exception(Error::IO, "could not rename image file %1+ to %1").arg(m_applicationImageFile);
    }

    // from this point onwards, we are not allowed to throw anymore, since the installation is "done"

    setState(CleaningUp);

    renameApplication.take();
    renameManifest.take();
    documentDirCreator.take();

    m_imageCreator.take();
    m_installationDirCreator.take();
    m_manifestDirPlusCreator.take();

    // this should not be necessary, but it also won't hurt
    if (mode == Update)
        removeRecursiveHelper(m_applicationDir.absolutePath() + qL1C('-'));

#ifdef Q_OS_UNIX
    // write files to the filesystem
    sync();
#endif

    m_errorString.clear();
}

QT_END_NAMESPACE_AM
