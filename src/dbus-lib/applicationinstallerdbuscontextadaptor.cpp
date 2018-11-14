/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include "applicationinstallerdbuscontextadaptor.h"
#include "applicationinstaller.h"
#include "io.qt.applicationinstaller_adaptor.h"
#include "dbuspolicy.h"
#include "exception.h"
#include "logging.h"


QT_BEGIN_NAMESPACE_AM

static QString taskStateToString(AsynchronousTask::TaskState state)
{
    const char *cstr = QMetaEnum::fromType<AsynchronousTask::TaskState>().valueToKey(state);
    return QString::fromUtf8(cstr);
}

ApplicationInstallerDBusContextAdaptor::ApplicationInstallerDBusContextAdaptor(ApplicationInstaller *ai)
    : AbstractDBusContextAdaptor(ai)
{
    m_adaptor = new ApplicationInstallerAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

ApplicationInstallerAdaptor::ApplicationInstallerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto ai = ApplicationInstaller::instance();

    connect(ai, &ApplicationInstaller::packageActivated,
            this, &ApplicationInstallerAdaptor::packageActivated);
    connect(ai, &ApplicationInstaller::packageDeactivated,
            this, &ApplicationInstallerAdaptor::packageDeactivated);
    connect(ai, &ApplicationInstaller::taskBlockingUntilInstallationAcknowledge,
            this, &ApplicationInstallerAdaptor::taskBlockingUntilInstallationAcknowledge);
    connect(ai, &ApplicationInstaller::taskFailed,
            this, &ApplicationInstallerAdaptor::taskFailed);
    connect(ai, &ApplicationInstaller::taskFinished,
            this, &ApplicationInstallerAdaptor::taskFinished);
    connect(ai, &ApplicationInstaller::taskProgressChanged,
            this, &ApplicationInstallerAdaptor::taskProgressChanged);
    connect(ai, &ApplicationInstaller::taskRequestingInstallationAcknowledge,
            this, &ApplicationInstallerAdaptor::taskRequestingInstallationAcknowledge);
    connect(ai, &ApplicationInstaller::taskStarted,
            this, &ApplicationInstallerAdaptor::taskStarted);
    connect(ai, &ApplicationInstaller::taskStateChanged,
            [this](const QString &taskId, AsynchronousTask::TaskState newState) {
                emit taskStateChanged(taskId, taskStateToString(newState));
            });
}

ApplicationInstallerAdaptor::~ApplicationInstallerAdaptor()
{ }

bool ApplicationInstallerAdaptor::allowInstallationOfUnsignedPackages() const
{
    return ApplicationInstaller::instance()->allowInstallationOfUnsignedPackages();
}

bool ApplicationInstallerAdaptor::applicationUserIdSeparation() const
{
    return ApplicationInstaller::instance()->isApplicationUserIdSeparationEnabled();
}

uint ApplicationInstallerAdaptor::commonApplicationGroupId() const
{
    return ApplicationInstaller::instance()->commonApplicationGroupId();
}

bool ApplicationInstallerAdaptor::developmentMode() const
{
    return ApplicationInstaller::instance()->developmentMode();
}

void ApplicationInstallerAdaptor::acknowledgePackageInstallation(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(void)
    return ApplicationInstaller::instance()->acknowledgePackageInstallation(taskId);
}

bool ApplicationInstallerAdaptor::activatePackage(const QString &id)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationInstaller::instance()->activatePackage(id);
}

bool ApplicationInstallerAdaptor::cancelTask(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationInstaller::instance()->cancelTask(taskId);
}

int ApplicationInstallerAdaptor::compareVersions(const QString &version1, const QString &version2)
{
    AM_AUTHENTICATE_DBUS(int)
    return ApplicationInstaller::instance()->compareVersions(version1, version2);
}

bool ApplicationInstallerAdaptor::deactivatePackage(const QString &id)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationInstaller::instance()->deactivatePackage(id);
}

bool ApplicationInstallerAdaptor::doesPackageNeedActivation(const QString &id)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationInstaller::instance()->doesPackageNeedActivation(id);
}

QVariantMap ApplicationInstallerAdaptor::getInstallationLocation(const QString &installationLocationId)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    return ApplicationInstaller::instance()->getInstallationLocation(installationLocationId);
}

QString ApplicationInstallerAdaptor::installationLocationIdFromApplication(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QString)
    return ApplicationInstaller::instance()->installationLocationIdFromApplication(id);
}

QStringList ApplicationInstallerAdaptor::installationLocationIds()
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return ApplicationInstaller::instance()->installationLocationIds();
}

qlonglong ApplicationInstallerAdaptor::installedApplicationSize(const QString &id)
{
    AM_AUTHENTICATE_DBUS(qlonglong)
    return ApplicationInstaller::instance()->installedApplicationSize(id);
}

QVariantMap ApplicationInstallerAdaptor::installedApplicationExtraMetaData(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    return ApplicationInstaller::instance()->installedApplicationExtraMetaData(id);
}

QVariantMap ApplicationInstallerAdaptor::installedApplicationExtraSignedMetaData(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    return ApplicationInstaller::instance()->installedApplicationExtraSignedMetaData(id);
}

bool ApplicationInstallerAdaptor::isPackageActivated(const QString &id)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationInstaller::instance()->isPackageActivated(id);
}

QString ApplicationInstallerAdaptor::removePackage(const QString &id, bool keepDocuments)
{
    return removePackage(id, keepDocuments, false);
}

QString ApplicationInstallerAdaptor::removePackage(const QString &id, bool keepDocuments, bool force)
{
    AM_AUTHENTICATE_DBUS(QString)
    return ApplicationInstaller::instance()->removePackage(id, keepDocuments, force);
}

QString ApplicationInstallerAdaptor::startPackageInstallation(const QString &installationLocationId, const QString &sourceUrl)
{
    AM_AUTHENTICATE_DBUS(QString)
    return ApplicationInstaller::instance()->startPackageInstallation(installationLocationId, sourceUrl);
}

QString ApplicationInstallerAdaptor::taskState(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)
    return taskStateToString(ApplicationInstaller::instance()->taskState(taskId));
}

QString ApplicationInstallerAdaptor::taskApplicationId(const QString &taskId)
{
    AM_AUTHENTICATE_DBUS(QString)
    return ApplicationInstaller::instance()->taskApplicationId(taskId);
}

QStringList ApplicationInstallerAdaptor::activeTaskIds()
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return ApplicationInstaller::instance()->activeTaskIds();
}
