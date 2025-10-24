// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DEVMODE_H
#define DEVMODE_H

#include <QtTest>
#include <packagemanager.h>

QT_USE_NAMESPACE_AM

#if defined(QT_BUILD_INTERNAL)
Q_AUTOTEST_EXPORT extern void qtam_PackageManager_forceUnlockConfiguration();
#endif

class DevMode
{
public:
    DevMode(PackageManager::DevelopmentMode mode, bool allowUnsigned = false)
        : m_oldMode(PackageManager::instance()->developmentMode())
    {
#if defined(QT_BUILD_INTERNAL)
        qtam_PackageManager_forceUnlockConfiguration();
        PackageManager::instance()->setDevelopmentMode(mode);
        PackageManager::instance()->setAllowInstallationOfUnsignedPackages(allowUnsigned);
#else
        QSKIP("This test requires a developer-build");
        Q_UNUSED(mode)
        Q_UNUSED(allowUnsigned)
        Q_UNUSED(m_oldMode)
        Q_UNUSED(m_oldUnsigned)
#endif
    }
    ~DevMode()
    {
#if defined(QT_BUILD_INTERNAL)
        PackageManager::instance()->setDevelopmentMode(m_oldMode);
        PackageManager::instance()->setAllowInstallationOfUnsignedPackages(m_oldUnsigned);
#endif
    }

private:
    PackageManager::DevelopmentMode m_oldMode;
    bool m_oldUnsigned;
};

#endif // DEVMODE_H
