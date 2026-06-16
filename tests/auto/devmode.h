// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DEVMODE_H
#define DEVMODE_H

#include <QtTest>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <packagemanager.h>

QT_USE_NAMESPACE_AM

using namespace Qt::StringLiterals;

// RAII helper that switches the PackageManager into a given development mode for the duration of a
// test and restores the previous state afterwards. When a PKCS#12 file is passed, it additionally
// sets the developer certificate (which only works in DevelopmentMode::Application) and makes sure
// the persisted development-mode.ini is removed again on destruction.
class DevMode
{
public:
    DevMode(PackageManager::DevelopmentMode mode, bool allowUnsigned = false,
            const QString &pkcs12File = { }, const QByteArray &pkcs12Password = { })
        : m_oldMode(PackageManager::instance()->developmentMode())
        , m_oldUnsigned(PackageManager::instance()->allowInstallationOfUnsignedPackages())
    {
#if defined(QT_BUILD_INTERNAL)
        qtam_PackageManager_forceUnlockConfiguration();
        PackageManager::instance()->setDevelopmentMode(mode);
        PackageManager::instance()->setAllowInstallationOfUnsignedPackages(allowUnsigned);

        if (!pkcs12File.isEmpty()) {
            QFile f(pkcs12File);
            if (f.open(QIODevice::ReadOnly))
                PackageManager::instance()->setDeveloperCertificate(f.readAll(), pkcs12Password);
        }
#else
        QSKIP("This test requires a developer-build");
        Q_UNUSED(mode)
        Q_UNUSED(allowUnsigned)
        Q_UNUSED(pkcs12File)
        Q_UNUSED(pkcs12Password)
        Q_UNUSED(m_oldMode)
        Q_UNUSED(m_oldUnsigned)
#endif
    }
    ~DevMode()
    {
#if defined(QT_BUILD_INTERNAL)
        // clear any developer certificate we might have set (only possible while in Application mode)
        if (PackageManager::instance()->developmentMode() == PackageManager::DevelopmentMode::Application)
            PackageManager::instance()->setDeveloperCertificate({ }, { });

        PackageManager::instance()->setDevelopmentMode(m_oldMode);
        PackageManager::instance()->setAllowInstallationOfUnsignedPackages(m_oldUnsigned);

        // get rid of the persisted signature, so the next test starts from a clean state
        QDir dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QFile::remove(dataDir.absoluteFilePath(u"development-mode.ini"_s));
#endif
    }

private:
    PackageManager::DevelopmentMode m_oldMode;
    bool m_oldUnsigned = false;
};

#endif // DEVMODE_H
