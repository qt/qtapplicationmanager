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

#include <qglobal.h>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

#include "applicationmanagerdbuscontextadaptor.h"
#include "applicationmanager.h"
#include "applicationmanager_adaptor.h"
#include "dbuspolicy.h"
#include "exception.h"
#include "logging.h"



QT_BEGIN_NAMESPACE_AM

ApplicationManagerDBusContextAdaptor::ApplicationManagerDBusContextAdaptor(ApplicationManager *am)
    : AbstractDBusContextAdaptor(am)
{
    m_adaptor = new ApplicationManagerAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

ApplicationManagerAdaptor::ApplicationManagerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto am = ApplicationManager::instance();

    connect(am, &ApplicationManager::countChanged,
            this, &ApplicationManagerAdaptor::countChanged);
    connect(am, &ApplicationManager::applicationAdded,
            this, &ApplicationManagerAdaptor::applicationAdded);
    connect(am, &ApplicationManager::applicationChanged,
            this, &ApplicationManagerAdaptor::applicationChanged);
    connect(am, &ApplicationManager::applicationAboutToBeRemoved,
            this, &ApplicationManagerAdaptor::applicationAboutToBeRemoved);
    connect(am, &ApplicationManager::applicationWasActivated,
            this, &ApplicationManagerAdaptor::applicationWasActivated);
    connect(am, &ApplicationManager::windowManagerCompositorReadyChanged,
            this, &ApplicationManagerAdaptor::windowManagerCompositorReadyChanged);

    // connect this signal via a lambda, since it needs a type conversion
    connect(am, &ApplicationManager::applicationRunStateChanged,
            this, [this](const QString &id, QT_PREPEND_NAMESPACE_AM(Am::RunState) runState) {
        emit applicationRunStateChanged(id, runState);
    });
}

ApplicationManagerAdaptor::~ApplicationManagerAdaptor()
{ }

int ApplicationManagerAdaptor::count() const
{
    return ApplicationManager::instance()->count();
}

bool ApplicationManagerAdaptor::dummy() const
{
    return ApplicationManager::instance()->isDummy();
}

bool ApplicationManagerAdaptor::securityChecksEnabled() const
{
    return ApplicationManager::instance()->securityChecksEnabled();
}

bool ApplicationManagerAdaptor::singleProcess() const
{
    return ApplicationManager::instance()->isSingleProcess();
}

QVariantMap ApplicationManagerAdaptor::systemProperties() const
{
    return convertFromJSVariant(ApplicationManager::instance()->systemProperties()).toMap();
}

bool ApplicationManagerAdaptor::windowManagerCompositorReady() const
{
    return ApplicationManager::instance()->isWindowManagerCompositorReady();
}

QStringList ApplicationManagerAdaptor::applicationIds()
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return ApplicationManager::instance()->applicationIds();
}

uint ApplicationManagerAdaptor::applicationRunState(const QString &id)
{
    AM_AUTHENTICATE_DBUS(uint)
    return ApplicationManager::instance()->applicationRunState(id);
}

QStringList ApplicationManagerAdaptor::capabilities(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return ApplicationManager::instance()->capabilities(id);
}

bool ApplicationManagerAdaptor::debugApplication(const QString &id, const QString &debugWrapper)
{
    return debugApplication(id, debugWrapper, QString());
}

bool ApplicationManagerAdaptor::debugApplication(const QString &id, const QString &debugWrapper, const QString &documentUrl)
{
    return debugApplication(id, debugWrapper, UnixFdMap(), documentUrl);
}

bool ApplicationManagerAdaptor::debugApplication(const QString &id, const QString &debugWrapper, const QtAM::UnixFdMap &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> stdioRedirections = { -1, -1, -1 };

#  if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            stdioRedirections[0] = dup(fd);
        else if (which == qL1S("out"))
            stdioRedirections[1] = dup(fd);
        else if (which == qL1S("err"))
            stdioRedirections[2] = dup(fd);
    }
#  else
    Q_UNUSED(redirections)
#  endif // defined(Q_OS_UNIX)
    auto am = ApplicationManager::instance();
    try {
        return am->startApplicationInternal(id, documentUrl, QString(), debugWrapper,
                                            std::move(stdioRedirections));
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        AbstractDBusContextAdaptor::dbusContextFor(this)->sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
        return false;
    }
}

QVariantMap ApplicationManagerAdaptor::get(const QString &id)
{
    AM_AUTHENTICATE_DBUS(QVariantMap)
    auto map = ApplicationManager::instance()->get(id);
    map.remove(qSL("application")); // cannot marshall QObject *
    return map;
}

QString ApplicationManagerAdaptor::identifyApplication(qlonglong pid)
{
    AM_AUTHENTICATE_DBUS(QString)
    return ApplicationManager::instance()->identifyApplication(pid);
}

QStringList ApplicationManagerAdaptor::identifyAllApplications(qlonglong pid)
{
    AM_AUTHENTICATE_DBUS(QStringList)
    return ApplicationManager::instance()->identifyAllApplications(pid);
}

bool ApplicationManagerAdaptor::openUrl(const QString &url)
{
    AM_AUTHENTICATE_DBUS(bool)
    return ApplicationManager::instance()->openUrl(url);
}

bool ApplicationManagerAdaptor::startApplication(const QString &id)
{
    return startApplication(id, QString());
}

bool ApplicationManagerAdaptor::startApplication(const QString &id, const QString &documentUrl)
{
    return startApplication(id, UnixFdMap(), documentUrl);
}

bool ApplicationManagerAdaptor::startApplication(const QString &id, const QtAM::UnixFdMap &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> stdioRedirections = { -1, -1, -1 };

#  if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            stdioRedirections[0] = dup(fd);
        else if (which == qL1S("out"))
            stdioRedirections[1] = dup(fd);
        else if (which == qL1S("err"))
            stdioRedirections[2] = dup(fd);
    }
#  else
    Q_UNUSED(redirections)
#  endif // defined(Q_OS_UNIX)
    auto am = ApplicationManager::instance();
    try {
        return am->startApplicationInternal(id, documentUrl, QString(), QString(),
                                            std::move(stdioRedirections));
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        AbstractDBusContextAdaptor::dbusContextFor(this)->sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
        return false;
    }
}

void ApplicationManagerAdaptor::stopAllApplications()
{
    stopAllApplications(false);
}

void ApplicationManagerAdaptor::stopAllApplications(bool forceKill)
{
    AM_AUTHENTICATE_DBUS(void)
    ApplicationManager::instance()->stopAllApplications(forceKill);
}

void ApplicationManagerAdaptor::stopApplication(const QString &id)
{
    stopApplication(id, false);
}

void ApplicationManagerAdaptor::stopApplication(const QString &id, bool forceKill)
{
    AM_AUTHENTICATE_DBUS(void)
    ApplicationManager::instance()->stopApplication(id, forceKill);
}
