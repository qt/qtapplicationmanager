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

#include <QTemporaryDir>
#include <QMessageAuthenticationCode>
#include <QPointer>

#include "logging.h"
#include "packagemanager_p.h"
#include "package.h"
#include "packageinfo.h"
#include "packageextractor.h"
#include "exception.h"
#include "packagemanager.h"
#include "sudo.h"
#include "utilities.h"
#include "signature.h"
#include "sudo.h"
#include "installationtask.h"

#include <memory>

/*
  Overview of what happens on an installation of an app with <id> to <location>:

  Step 1 -- startInstallation()
  =============================

  delete <location>/<id>+

  create dir <location>/<id>+
  set <extractiondir> to <location>/<id>+


  Step 2 -- unpack files
  ======================

  PackageExtractor does its job


  Step 3 -- finishInstallation()
  ================================

  if (exists <location>/<id>)
      set <isupdate> to <true>

  create installation report at <extractiondir>/.installation-report.yaml

  if (not <isupdate>)
      create document directory

  if (optional uid separation)
      chown/chmod recursively in <extractiondir> and document directory


  Step 3.1 -- final rename in finishInstallation()
  ==================================================

  if (<isupdate>)
      rename <location>/<id> to <location>/<id>-
  rename <location>/<id>+ to <location>/<id>
*/

QT_BEGIN_NAMESPACE_AM



// The standard QTemporaryDir destructor cannot cope with read-only sub-directories.
class TemporaryDir : public QTemporaryDir
{
public:
    TemporaryDir()
        : QTemporaryDir()
    { }
    explicit TemporaryDir(const QString &templateName)
        : QTemporaryDir(templateName)
    { }
    ~TemporaryDir()
    {
        recursiveOperation(path(), safeRemove);
    }
private:
    Q_DISABLE_COPY(TemporaryDir)
};


QMutex InstallationTask::s_serializeFinishInstallation { };

InstallationTask::InstallationTask(const QString &installationPath, const QString &documentPath,
                                   const QUrl &sourceUrl, QObject *parent)
    : AsynchronousTask(parent)
    , m_pm(PackageManager::instance())
    , m_installationPath(installationPath)
    , m_documentPath(documentPath)
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
        if (m_installationPath.isEmpty())
            throw Exception("no installation location was configured");

        TemporaryDir extractionDir;
        if (!extractionDir.isValid())
            throw Exception("could not create a temporary extraction directory");

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
            throw Exception(Error::Package, "package did not contain a valid info.yaml and icon file");

        QList<QByteArray> chainOfTrust = m_pm->caCertificates();

        if (!m_pm->allowInstallationOfUnsignedPackages()) {
            if (!m_extractor->installationReport().storeSignature().isEmpty()) {
                // normal package from the store
                QByteArray sigDigest = m_extractor->installationReport().digest();
                bool sigOk = false;

                if (Signature(sigDigest).verify(m_extractor->installationReport().storeSignature(), chainOfTrust)) {
                    sigOk = true;
                } else if (!m_pm->hardwareId().isEmpty()) {
                    // did not verify - if we have a hardware-id, try to verify with it
                    sigDigest = QMessageAuthenticationCode::hash(sigDigest, m_pm->hardwareId().toUtf8(), QCryptographicHash::Sha256);
                    if (Signature(sigDigest).verify(m_extractor->installationReport().storeSignature(), chainOfTrust))
                        sigOk = true;
                }
                if (!sigOk)
                    throw Exception(Error::Package, "could not verify the package's store signature");
            } else if (!m_extractor->installationReport().developerSignature().isEmpty()) {
                // developer package - needs a device in dev mode
                if (!m_pm->developmentMode())
                    throw Exception(Error::Package, "cannot install development packages on consumer devices");

                if (!Signature(m_extractor->installationReport().digest()).verify(m_extractor->installationReport().developerSignature(), chainOfTrust))
                    throw Exception(Error::Package, "could not verify the package's developer signature");

            } else {
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

        // However many downloads are allowed to happen in parallel: we need to serialize those
        // tasks here for the finishInstallation() step
        QMutexLocker finishLocker(&s_serializeFinishInstallation);

        finishInstallation();

        // At this point, the installation is done, so we cannot throw anymore.

        // we need to call those PackageManager methods in the correct thread
        bool finishOk = false;
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &finishOk]()
            { finishOk = PackageManager::instance()->finishedPackageInstall(m_packageId); },
            Qt::BlockingQueuedConnection);

        if (!finishOk)
            qCWarning(LogInstaller) << "PackageManager rejected the installation of " << m_packageId;

    } catch (const Exception &e) {
        setError(e.errorCode(), e.errorString());

        if (m_managerApproval) {
            // we need to call those ApplicationManager methods in the correct thread
            bool cancelOk = false;
            QMetaObject::invokeMethod(PackageManager::instance(), [this, &cancelOk]()
                { cancelOk = PackageManager::instance()->canceledPackageInstall(m_packageId); },
                Qt::BlockingQueuedConnection);

            if (!cancelOk)
                qCWarning(LogInstaller) << "PackageManager could not remove package" << m_packageId << "after a failed installation";
        }
    }


    {
        QMutexLocker locker(&m_mutex);
        delete m_extractor;
        m_extractor = nullptr;
    }
}


void InstallationTask::checkExtractedFile(const QString &file) Q_DECL_NOEXCEPT_EXPR(false)
{
    ++m_extractedFileCount;

    if (m_extractedFileCount == 1) {
        if (file != qL1S("info.yaml"))
            throw Exception(Error::Package, "info.yaml must be the first file in the package. Got %1")
                .arg(file);

        m_package.reset(PackageInfo::fromManifest(m_extractor->destinationDirectory().absoluteFilePath(file)));
        if (m_package->id() != m_extractor->installationReport().packageId())
            throw Exception(Error::Package, "the package identifiers in --PACKAGE-HEADER--' and info.yaml do not match");

        m_iconFileName = m_package->icon(); // store it separately as we will give away ApplicationInfo later on

        if (m_iconFileName.isEmpty())
            throw Exception(Error::Package, "the 'icon' field in info.yaml cannot be empty or absent.");

        m_packageId = m_package->id();

        m_foundInfo = true;
    } else if (m_extractedFileCount == 2) {
        // the second file must be the icon

        Q_ASSERT(m_foundInfo);
        Q_ASSERT(!m_foundIcon);

        if (file != m_iconFileName)
            throw Exception(Error::Package,
                    "The package icon (as stated in info.yaml) must be the second file in the package."
                    " Expected '%1', got '%2'").arg(m_iconFileName, file);

        QFile icon(m_extractor->destinationDirectory().absoluteFilePath(file));

        if (icon.size() > 256*1024)
            throw Exception(Error::Package, "the size of %1 is too large (max. 256KB)").arg(file);

        m_foundIcon = true;
    } else {
        throw Exception(Error::Package, "Could not find info.yaml and the icon file at the beginning of the package.");
    }

    if (m_foundIcon && m_foundInfo) {
        bool doubleInstallation = false;
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &doubleInstallation]() {
            doubleInstallation = PackageManager::instance()->isPackageInstallationActive(m_packageId);
        }, Qt::BlockingQueuedConnection);
        if (doubleInstallation)
            throw Exception(Error::Package, "Cannot install the same package %1 multiple times in parallel").arg(m_packageId);

        qCDebug(LogInstaller) << "emit taskRequestingInstallationAcknowledge" << id() << "for package" << m_package->id();

        // this is a temporary just for the signal emission below
        m_tempPackageForAcknowledge.reset(new Package(m_package.get(), Package::BeingInstalled));
        emit m_pm->taskRequestingInstallationAcknowledge(id(), m_tempPackageForAcknowledge.get(),
                                                         m_extractor->installationReport().extraMetaData(),
                                                         m_extractor->installationReport().extraSignedMetaData());

        QDir oldDestinationDirectory = m_extractor->destinationDirectory();

        startInstallation();

        QFile::copy(oldDestinationDirectory.filePath(qSL("info.yaml")), m_extractionDir.filePath(qSL("info.yaml")));
        QFile::copy(oldDestinationDirectory.filePath(m_iconFileName), m_extractionDir.filePath(m_iconFileName));

        {
            QMutexLocker locker(&m_mutex);
            m_extractor->setDestinationDirectory(m_extractionDir);

            QString path = m_extractionDir.absolutePath();
            path.chop(1); // remove the '+'
            m_package->setBaseDir(QDir(path));
        }
        // we need to find a free uid before we call startingApplicationInstallation
        m_package->m_uid = m_pm->findUnusedUserId();
        m_applicationUid = m_package->m_uid;

        // we need to call those ApplicationManager methods in the correct thread
        // this will also exclusively lock the application for us
        // m_package ownership is transferred to the ApplicationManager
        QString packageId = m_package->id(); // m_package is gone after the invoke
        QPointer<Package> newPackage;
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &newPackage]()
            { newPackage = PackageManager::instance()->startingPackageInstallation(m_package.release()); },
            Qt::BlockingQueuedConnection);
        m_managerApproval = !newPackage.isNull();

        if (!m_managerApproval)
            throw Exception("PackageManager declined the installation of %1").arg(packageId);

        // if any of the apps in the package were running before, we now need to wait until all of
        // them have actually stopped
        while (!m_canceled && newPackage && !newPackage->areAllApplicationsStoppedDueToBlock())
            QThread::msleep(30);

        if (m_canceled || newPackage.isNull())
            throw Exception(Error::Canceled, "canceled");

        // we're not interested in any other files from here on...
        m_extractor->setFileExtractedCallback(nullptr);
    }
}

void InstallationTask::startInstallation() Q_DECL_NOEXCEPT_EXPR(false)
{
    // 2. delete old, partial installation

    QDir installationDir = QString(m_installationPath + qL1C('/'));
    QString installationTarget = m_packageId + qL1C('+');
    if (installationDir.exists(installationTarget)) {
        if (!removeRecursiveHelper(installationDir.absoluteFilePath(installationTarget)))
            throw Exception("could not remove old, partial installation %1/%2").arg(installationDir).arg(installationTarget);
    }

    // 4. create new installation
    if (!m_installationDirCreator.create(installationDir.absoluteFilePath(installationTarget)))
        throw Exception("could not create installation directory %1/%2").arg(installationDir).arg(installationTarget);
    m_extractionDir = installationDir;
    if (!m_extractionDir.cd(installationTarget))
        throw Exception("could not cd into installation directory %1/%2").arg(installationDir).arg(installationTarget);
    m_applicationDir.setPath(installationDir.absoluteFilePath(m_packageId));
}

void InstallationTask::finishInstallation() Q_DECL_NOEXCEPT_EXPR(false)
{
    QDir documentDirectory(m_documentPath);
    ScopedDirectoryCreator documentDirCreator;

    enum { Installation, Update } mode = Installation;

    if (m_applicationDir.exists())
        mode = Update;

    // create the installation report
    InstallationReport report = m_extractor->installationReport();

    QFile reportFile(m_extractionDir.absoluteFilePath(qSL(".installation-report.yaml")));
    if (!reportFile.open(QFile::WriteOnly) || !report.serialize(&reportFile))
        throw Exception(reportFile, "could not write the installation report");
    reportFile.close();

    // create the document directories when installing (not needed on updates)
    if ((mode == Installation) && !m_documentPath.isEmpty()) {
        // this package may have been installed earlier and the document directory may not have been removed
        if (!documentDirectory.cd(m_packageId)) {
            if (!documentDirCreator.create(documentDirectory.absoluteFilePath(m_packageId)))
                throw Exception(Error::IO, "could not create the document directory %1").arg(documentDirectory.filePath(m_packageId));
        }
    }
#ifdef Q_OS_UNIX
    // update the owner, group and permission bits on both the installation and document directories
    SudoClient *root = SudoClient::instance();

    if (m_pm->isApplicationUserIdSeparationEnabled() && root) {
        uid_t uid = m_applicationUid;
        gid_t gid = m_pm->commonApplicationGroupId();

        if (!root->setOwnerAndPermissionsRecursive(documentDirectory.filePath(m_packageId), uid, gid, 02700)) {
            throw Exception(Error::IO, "could not recursively change the owner to %1:%2 and the permission bits to %3 in %4")
                    .arg(uid).arg(gid).arg(02700, 0, 8).arg(documentDirectory.filePath(m_packageId));
        }

        if (!root->setOwnerAndPermissionsRecursive(m_extractionDir.path(), uid, gid, 0440)) {
            throw Exception(Error::IO, "could not recursively change the owner to %1:%2 and the permission bits to %3 in %4")
                    .arg(uid).arg(gid).arg(0440, 0, 8).arg(m_extractionDir.absolutePath());
        }
    }
#endif

    // final rename

    // POSIX cannot atomically rename directories, if the destination directory exists
    // and is non-empty. We need to do a double-rename in this case, which might fail!
    // The image is a file, so this limitation does not apply!

    ScopedRenamer renameApplication;

    if (mode == Update) {
        if (!renameApplication.rename(m_applicationDir, ScopedRenamer::NamePlusToName | ScopedRenamer::NameToNameMinus))
            throw Exception(Error::IO, "could not rename application directory %1+ to %1 (including a backup to %1-)").arg(m_applicationDir);
    } else {
        if (!renameApplication.rename(m_applicationDir, ScopedRenamer::NamePlusToName))
            throw Exception(Error::IO, "could not rename application directory %1+ to %1").arg(m_applicationDir);
    }

    // from this point onwards, we are not allowed to throw anymore, since the installation is "done"

    setState(CleaningUp);

    renameApplication.take();
    documentDirCreator.take();

    m_installationDirCreator.take();

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

#include "moc_installationtask.cpp"
