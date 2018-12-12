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

#include <QDBusConnection>

#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/logging.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManCommon/crashhandler.h>
#include <QtAppManCommon/exception.h>

#include <QtAppManCommon/qtyaml.h>

#if !defined(AM_HEADLESS) && defined(QT_WAYLANDCLIENT_LIB)
#  include <QWindow>
#  include "waylandqtamclientextension_p.h"
#endif
#include "launchermain.h"

QT_BEGIN_NAMESPACE_AM

LauncherMain::LauncherMain(int &argc, char **argv) Q_DECL_NOEXCEPT
    : LauncherMainBase(SharedMain::preConstructor(argc), argv)
    , SharedMain()
{ }

LauncherMain::~LauncherMain()
{ }

LauncherMain *LauncherMain::instance()
{
    return qobject_cast<LauncherMain *>(qApp);
}

void LauncherMain::registerWaylandExtensions() Q_DECL_NOEXCEPT
{
#if !defined(AM_HEADLESS) && defined(QT_WAYLANDCLIENT_LIB)
     m_waylandExtension = new WaylandQtAMClientExtension();
     connect(m_waylandExtension, &WaylandQtAMClientExtension::windowPropertyChanged,
             this, &LauncherMain::windowPropertyChanged);
#endif
}

QString LauncherMain::baseDir() const
{
    return m_baseDir;
}

QVariantMap LauncherMain::runtimeConfiguration() const
{
    return m_runtimeConfiguration;
}

QByteArray LauncherMain::securityToken() const
{
    return m_securityToken;
}

bool LauncherMain::slowAnimations() const
{
    return m_slowAnimations;
}

QVariantMap LauncherMain::systemProperties() const
{
    return m_systemProperties;
}

QStringList LauncherMain::loggingRules() const
{
    return m_loggingRules;
}

QString LauncherMain::p2pDBusName() const
{
    return qSL("am");
}

QString LauncherMain::notificationDBusName() const
{
    return m_dbusAddressNotifications.isEmpty() ? QString() : qSL("am_notification_bus");
}

QVariantMap LauncherMain::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

QString LauncherMain::iconThemeName() const
{
    return m_iconThemeName;
}

QStringList LauncherMain::iconThemeSearchPaths() const
{
    return m_iconThemeSearchPaths;
}

QVariantMap LauncherMain::windowProperties(QWindow *window) const
{
#if !defined(AM_HEADLESS) && defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        return m_waylandExtension->windowProperties(window);
#else
    Q_UNUSED(window)
#endif
    return QVariantMap();
}

void LauncherMain::setWindowProperty(QWindow *window, const QString &name, const QVariant &value)
{
#if !defined(AM_HEADLESS) && defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        m_waylandExtension->setWindowProperty(window, name, value);
#else
    Q_UNUSED(window)
    Q_UNUSED(name)
    Q_UNUSED(value)
#endif
}

void LauncherMain::clearWindowPropertyCache(QWindow *window)
{
#if !defined(AM_HEADLESS) && defined(QT_WAYLANDCLIENT_LIB)
    if (m_waylandExtension && window)
        m_waylandExtension->clearWindowPropertyCache(window);
#else
    Q_UNUSED(window)
#endif
}

QString LauncherMain::applicationId() const
{
    return property("__am_applicationId").toString();
}

void LauncherMain::setApplicationId(const QString &applicationId)
{
    setProperty("__am_applicationId", applicationId);
}

void LauncherMain::loadConfiguration(const QByteArray &configYaml) Q_DECL_NOEXCEPT_EXPR(false)
{
    auto docs = QtYaml::variantDocumentsFromYaml(configYaml.isEmpty() ? qgetenv("AM_CONFIG")
                                                                      : configYaml);
    if (docs.size() == 1)
        m_configuration = docs.first().toMap();

    m_baseDir = m_configuration.value(qSL("baseDir")).toString() + qL1C('/');
    m_runtimeConfiguration = m_configuration.value(qSL("runtimeConfiguration")).toMap();
    m_securityToken = QByteArray::fromHex(m_configuration.value(qSL("")).toString().toLatin1());
    m_systemProperties = m_configuration.value(qSL("systemProperties")).toMap();

    QVariantMap loggingConfig = m_configuration.value(qSL("logging")).toMap();
    m_loggingRules = variantToStringList(loggingConfig.value(qSL("rules")));

    QVariantMap dbusConfig = m_configuration.value(qSL("dbus")).toMap();
    m_dbusAddressP2P = dbusConfig.value(qSL("p2p")).toString();
    m_dbusAddressNotifications = dbusConfig.value(qSL("org.freedesktop.Notifications")).toString();

    QVariantMap uiConfig = m_configuration.value(qSL("ui")).toMap();
    m_slowAnimations = uiConfig.value(qSL("slowAnimations")).toBool();
    m_openGLConfiguration = uiConfig.value(qSL("opengl")).toMap();
    m_iconThemeName = uiConfig.value(qSL("iconThemeName")).toString();
    m_iconThemeSearchPaths = uiConfig.value(qSL("iconThemeSearchPaths")).toStringList();

    // un-comment this if things go south:
    //qWarning() << "### LOG " << m_loggingRules;
    //qWarning() << "### DBUS" << dbusConfig;
    //qWarning() << "### UI  " << uiConfig;
    //qWarning() << "### RT  " << m_runtimeConfiguration;
    //qWarning() << "### SYSP" << m_systemProperties;
    //qWarning() << "### GL  " << m_openGLConfiguration;

    // sanity checks
    if (m_baseDir == qL1S("/"))
        throw Exception("Runtime launcher received an empty baseDir");
    if (loggingConfig.isEmpty())
        throw Exception("Runtime launcher received no logging configuration");
    if (dbusConfig.isEmpty())
        throw Exception("Runtime launcher received no D-Bus configuration");
}

void LauncherMain::setupDBusConnections() Q_DECL_NOEXCEPT_EXPR(false)
{
    if (m_dbusAddressP2P.isEmpty())
        throw Exception("No address available to connect to the P2P D-Bus");

    auto dbusConnection = QDBusConnection::connectToPeer(m_dbusAddressP2P, p2pDBusName());

    if (!dbusConnection.isConnected())
        throw Exception("could not connect to the P2P D-Bus via: %1").arg(m_dbusAddressP2P);

    qCDebug(LogQmlRuntime) << "Connected to the P2P D-Bus via:" << m_dbusAddressP2P;

    if (!m_dbusAddressNotifications.isEmpty()) {
        if (m_dbusAddressNotifications == qL1S("system"))
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SystemBus, notificationDBusName());
        else if (m_dbusAddressNotifications == qL1S("session"))
            dbusConnection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, notificationDBusName());
        else
            dbusConnection = QDBusConnection::connectToBus(m_dbusAddressNotifications, notificationDBusName());

        if (!dbusConnection.isConnected())
            throw Exception("could not connect to the Notification D-Bus via: %1").arg(m_dbusAddressNotifications);

        qCDebug(LogQmlRuntime) << "Connected to the Notification D-Bus via:" << m_dbusAddressNotifications;
    } else {
        qCWarning(LogDeployment) << "Notifications are not supported by this configuration";
    }
}

QT_END_NAMESPACE_AM
