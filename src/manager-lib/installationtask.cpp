// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <QTemporaryDir>
#include <QMessageAuthenticationCode>
#include <QPointer>
#include <QStandardPaths>

#include "logging.h"
#include "packagemanager_p.h"
#include "package.h"
#include "packageinfo.h"
#include "packageextractor.h"
#include "application.h"
#include "applicationinfo.h"
#include "exception.h"
#include "packagemanager.h"
#include "utilities.h"
#include "signature.h"
#include "sudo.h"
#include "installationtask.h"

#include <memory>
#ifdef Q_OS_UNIX
#  include <unistd.h>
#endif

using namespace Qt::StringLiterals;


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
        if (autoRemove())
            recursiveOperation(path(), safeRemove);
    }
private:
    Q_DISABLE_COPY_MOVE(TemporaryDir)
};


QMutex InstallationTask::s_serializeFinishInstallation { };

InstallationTask::InstallationTask(const QString &installationPath, const QString &documentPath,
                                   const QUrl &sourceUrl, QObject *parent)
    : AsynchronousTask(parent)
    , m_pm(PackageManager::instance())
    , m_installationPath(installationPath)
    , m_documentPath(documentPath)
    , m_sourceUrl(sourceUrl)
{
    setObjectName(u"QtAM-InstallationTask"_s);
}

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

        TemporaryDir extractionDir(m_installationPath + u"/.tmp-XXXXXX"_s);
        if (!extractionDir.isValid())
            throw Exception("could not create a temporary extraction directory at %1").arg(m_installationPath);

        // protect m_canceled and changes to m_extractor
        QMutexLocker locker(&m_mutex);
        if (m_canceled)
            throw Exception(Error::Canceled, "canceled");

        m_extractor = new PackageExtractor(m_sourceUrl, QDir(extractionDir.path()));
        locker.unlock();

        connect(m_extractor, &PackageExtractor::progress, this, &AsynchronousTask::progress);

        m_extractor->setFileExtractedCallback([this](const QString &f) { checkExtractedFile(f); });
        m_extractor->setExtendedAttributeCallback([](const QString &f, const QByteArray &name, const QByteArray &value) {
            if (!SudoClient::instance()->setExtendedAttribute(f, name, value)) {
                QByteArray who = SudoClient::instance()->isFallbackImplementation() ? "appman user" : "root";
                throw Exception("could not set extended attribute '%1' on file '%2' as %3: %4")
                        .arg(name).arg(f).arg(who).arg(SudoClient::instance()->lastError());
            }
        });

        if (!m_extractor->extract())
            throw Exception(m_extractor->errorCode(), m_extractor->errorString());

        if (!m_foundInfo || !m_foundIcon)
            throw Exception(Error::Package, "package did not contain a valid info.yaml and icon file");

        if (!m_pm->allowInstallationOfUnsignedPackages()) {
            bool hasStoreSignature = !m_extractor->installationReport().storeSignature().isEmpty();

            // Step 1: verify the store signature (optional, if in dev mode)
            if (hasStoreSignature) {
                // normal package from the store
                QByteArray sigDigest = m_extractor->installationReport().digest();

                Signature storeSig(sigDigest);
                storeSig.requireRevocationCheck(m_pm->certificateRevocationLists());
                storeSig.requireKeyUsage(Certificate::KeyUsage::Store);
                storeSig.requireIssuerFingerprint(Signature::FingerprintHash::Sha256,
                                                  m_pm->issuerCertificateFingerprintsStore());

                try {
                    (void) storeSig.verify(m_extractor->installationReport().storeSignature(),
                                           m_pm->caCertificatesCommon() + m_pm->caCertificatesStore());
                } catch (const Exception &e) {
                    if (!m_pm->hardwareId().isEmpty()) {
                        // did not verify - if we have a hardware-id, try to verify with it
                        sigDigest = QMessageAuthenticationCode::hash(sigDigest, m_pm->hardwareId().toUtf8(), QCryptographicHash::Sha256);
                        Signature storeHwidSig(sigDigest);
                        storeHwidSig.requireRevocationCheck(m_pm->certificateRevocationLists());
                        storeHwidSig.requireKeyUsage(Certificate::KeyUsage::Store);
                        storeHwidSig.requireIssuerFingerprint(Signature::FingerprintHash::Sha256,
                                                              m_pm->issuerCertificateFingerprintsStore());

                        try {
                            (void) storeHwidSig.verify(m_extractor->installationReport().storeSignature(),
                                                       m_pm->caCertificatesCommon() + m_pm->caCertificatesStore());
                        } catch (const Exception &e) {
                            throw Exception(Error::Package, "could not verify the package's store signature (with hardware-id): %1")
                                .arg(e.errorString());
                        }
                    } else {
                        throw Exception(Error::Package, "could not verify the package's store signature: %1")
                            .arg(e.errorString());
                    }
                }
            } else {
                if (!m_pm->developmentMode())
                    throw Exception(Error::Package, "cannot install packages without store signature on consumer devices");
            }

            // Step 2: verify the developer signature (required)
            if (!m_extractor->installationReport().developerSignature().isEmpty()) {
                if (!hasStoreSignature)
                    Q_ASSERT(m_pm->developmentMode());

                Signature devSig(m_extractor->installationReport().digest());
                devSig.requireRevocationCheck(m_pm->certificateRevocationLists());
                devSig.requireKeyUsage(Certificate::KeyUsage::Developer);
                devSig.requirePackageId(m_packageId);
                devSig.requireIssuerFingerprint(Signature::FingerprintHash::Sha256,
                                                m_pm->issuerCertificateFingerprintsDeveloper());

                try {
                    (void) devSig.verify(m_extractor->installationReport().developerSignature(),
                                         m_pm->caCertificatesCommon() + m_pm->caCertificatesDeveloper());
                } catch (const Exception &e) {
                    throw Exception(Error::Package, "could not verify the package's developer signature: %1")
                        .arg(e.errorString());
                }
            } else {
                if (hasStoreSignature)
                    throw Exception(Error::Package, "cannot install packages with only a store signature");
                else
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


void InstallationTask::checkExtractedFile(const QString &file) noexcept(false)
{
    ++m_extractedFileCount;

    if (m_extractedFileCount == 1) {
        if (file != u"info.yaml")
            throw Exception(Error::Package, "info.yaml must be the first file in the package. Got %1")
                .arg(file);

        m_package.reset(PackageInfo::fromManifest(m_extractor->destinationDirectory().absoluteFilePath(file)));
        if (m_package->id() != m_extractor->installationReport().packageId())
            throw Exception(Error::Package, "the package identifiers in --PACKAGE-HEADER--' and info.yaml do not match");

        m_iconFileName = m_package->icon(); // store it separately as we will transfer m_package ownership later
        if (m_iconFileName.isEmpty())
            m_foundIcon = true;
        else if (QFileInfo(m_iconFileName).path() != u'.')
            throw Exception(Error::Package, "the icon must be located in the package's root directory");

        m_mutex.lock();
        m_packageId = m_package->id();
        m_mutex.unlock();

        m_foundInfo = true;
    } else if (m_extractedFileCount == 2) {
        // the second file must be the icon

        Q_ASSERT(m_foundInfo);
        Q_ASSERT(!m_foundIcon);
        Q_ASSERT(!m_iconFileName.isEmpty());

        if (file != m_iconFileName)
            throw Exception(Error::Package,
                    "The package icon (as stated in info.yaml) must be the second file in the package."
                    " Expected '%1', got '%2'").arg(m_iconFileName, file);

        QFile icon(m_extractor->destinationDirectory().absoluteFilePath(file));
        if (icon.size() > 1024*1024)
            throw Exception(Error::Package, "the size of %1 is too large (max. 1MB)").arg(file);

        m_foundIcon = true;
    } else {
        throw Exception(Error::Package, "Could not find info.yaml and the icon file at the beginning of the package.");
    }

    if (m_foundIcon && m_foundInfo) {
        // we're not interested in any other files from here on...
        m_extractor->setFileExtractedCallback(nullptr);

        bool doubleInstallation = false;
        QMetaObject::invokeMethod(PackageManager::instance(), [this, &doubleInstallation]() {
            doubleInstallation = PackageManager::instance()->isPackageInstallationActive(m_packageId);
        }, Qt::BlockingQueuedConnection);
        if (doubleInstallation)
            throw Exception(Error::Package, "Cannot install the same package %1 multiple times in parallel").arg(m_packageId);

        QDir oldDestinationDirectory = m_extractor->destinationDirectory();

        startInstallation();

        QFile::rename(oldDestinationDirectory.filePath(u"info.yaml"_s), m_extractionDir.filePath(u"info.yaml"_s));
        if (!m_iconFileName.isEmpty())
            QFile::rename(oldDestinationDirectory.filePath(m_iconFileName), m_extractionDir.filePath(m_iconFileName));

        {
            QMutexLocker locker(&m_mutex);
            m_extractor->setDestinationDirectory(m_extractionDir);

            QString path = m_extractionDir.absolutePath();
            path.chop(1); // remove the '+'
            m_package->setBaseDir(QDir(path));
        }

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

        qCDebug(LogInstaller) << "emit taskRequestingInstallationAcknowledge" << id() << "for package" << packageId;

        // Create temporary objects for QML just for the signal emission.
        // The problem here is that the PackageInfo instance backing the Package object is also
        // temporary and the ownership is with the C++ side of the PackageManager.
        // Ideally we should have kept the 'package' parameter as a dumb QVariantMap, but changing
        // that back would be a huge API break nowadays as the QML APIs are fully typed.
        // At least we have to make sure NOT to change anything in the PackageInfo instance after
        // the signal emission below.
        m_tempPackageForAcknowledge.reset(new Package(newPackage->info(), Package::BeingInstalled));
        m_tempPackageForAcknowledge->moveToThread(m_pm->thread());
        const auto &applicationInfos = newPackage->info()->applications();
        for (const auto &applicationInfo : applicationInfos) {
            auto tempApp = new Application(applicationInfo, m_tempPackageForAcknowledge.get());
            tempApp->moveToThread(m_pm->thread());
            m_tempPackageForAcknowledge->addApplication(tempApp);
            m_tempApplicationsForAcknowledge.emplace_back(tempApp);
        }
        emit m_pm->taskRequestingInstallationAcknowledge(id(), m_tempPackageForAcknowledge.get(),
                                                         m_extractor->installationReport().extraMetaData(),
                                                         m_extractor->installationReport().extraSignedMetaData());

        // if any of the apps in the package were running before, we now need to wait until all of
        // them have actually stopped
        while (!m_canceled && newPackage && !newPackage->areAllApplicationsStoppedDueToBlock())
            QThread::msleep(30);

        if (m_canceled || newPackage.isNull())
            throw Exception(Error::Canceled, "canceled");
    }
}

void InstallationTask::startInstallation() noexcept(false)
{
    // 2. delete old, partial installation

    QDir installationDir = QString(m_installationPath + u'/');
    QString installationTarget = m_packageId + u'+';
    if (installationDir.exists(installationTarget)) {
        try {
            PackageManager::instance()->removeRecursive(installationDir.absoluteFilePath(installationTarget));
        } catch (const Exception &e) {
            throw Exception("could not remove old, partial installation %1/%2: %3")
                .arg(installationDir).arg(installationTarget).arg(e.errorString());
        }
    }

    // 4. create new installation
    if (!m_installationDirCreator.create(installationDir.absoluteFilePath(installationTarget)))
        throw Exception("could not create installation directory %1/%2").arg(installationDir).arg(installationTarget);
    m_extractionDir = installationDir;  // clazy:exclude=qt6-deprecated-api-fixes
    if (!m_extractionDir.cd(installationTarget))
        throw Exception("could not cd into installation directory %1/%2").arg(installationDir).arg(installationTarget);
    m_applicationDir.setPath(installationDir.absoluteFilePath(m_packageId));
}

void InstallationTask::finishInstallation() noexcept(false)
{
    QDir documentDirectory(m_documentPath);
    ScopedDirectoryCreator documentDirCreator;

    enum { Installation, Update } mode = Installation;

    if (m_applicationDir.exists())
        mode = Update;

    // create the installation report
    InstallationReport report = m_extractor->installationReport();

    QDir dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString instReport = dataDir.absoluteFilePath(u"installation-reports/"_s + m_packageId + u".yaml"_s);
    QFile reportFile(instReport + u'+');
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

    // final rename

    ScopedRenamer renameReport;
    if (!renameReport.rename(instReport, ScopedRenamer::NamePlusToName))
        throw Exception(Error::IO, "could not rename installation-report %1+ to %1 (including a backup to %1-)").arg(m_applicationDir);

    // POSIX cannot atomically rename directories, if the destination directory exists
    // and is non-empty. We need to do a double-rename in this case, which might fail!

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
    renameReport.take();
    documentDirCreator.take();

    m_installationDirCreator.take();

    // this should not be necessary, but it also won't hurt
    if (mode == Update) {
        try {
            PackageManager::instance()->removeRecursive(m_applicationDir.absolutePath() + u'-');
        } catch (...) { }
    }

#ifdef Q_OS_UNIX
    // write files to the filesystem
    sync();
#endif

    m_errorString.clear();
}

QT_END_NAMESPACE_AM

#include "moc_installationtask.cpp"
