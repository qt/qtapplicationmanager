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

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/sharedmain.h>

#if defined(AM_HEADLESS)
#  include <QCoreApplication>
typedef QCoreApplication LauncherMainBase;
#else
#  include <QGuiApplication>
typedef QGuiApplication LauncherMainBase;
#endif


QT_BEGIN_NAMESPACE_AM

class LauncherMain : public LauncherMainBase, public SharedMain
{
    Q_OBJECT
public:
    LauncherMain(int &argc, char **argv, const QByteArray &configYaml = QByteArray()) Q_DECL_NOEXCEPT_EXPR(false);
    ~LauncherMain();

public:
    void setupDBusConnections() Q_DECL_NOEXCEPT_EXPR(false);

    QString baseDir() const;
    QVariantMap runtimeConfiguration() const;
    QByteArray securityToken() const;
    bool slowAnimations() const;
    QVariantMap systemProperties() const;
    QStringList loggingRules() const;

    QString p2pDBusName() const;
    QString notificationDBusName() const;

    QVariantMap openGLConfiguration() const;

private:
    QVariantMap m_configuration;
    QString m_baseDir;
    QVariantMap m_runtimeConfiguration;
    QByteArray m_securityToken;
    bool m_slowAnimations = false;
    QVariantMap m_systemProperties;
    QStringList m_loggingRules;
    QString m_dbusAddressP2P;
    QString m_dbusAddressNotifications;
    QVariantMap m_openGLConfiguration;
};

QT_END_NAMESPACE_AM
