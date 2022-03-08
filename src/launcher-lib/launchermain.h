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

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/sharedmain.h>

QT_FORWARD_DECLARE_CLASS(QWindow)

QT_BEGIN_NAMESPACE_AM

class WaylandQtAMClientExtension;

class LauncherMain : public QObject, public SharedMain
{
    Q_OBJECT
public:
    LauncherMain() Q_DECL_NOEXCEPT;
    virtual ~LauncherMain();

    static LauncherMain *instance();

public:
    void loadConfiguration(const QByteArray &configYaml = QByteArray()) Q_DECL_NOEXCEPT_EXPR(false);
    void setupDBusConnections() Q_DECL_NOEXCEPT_EXPR(false);
    void registerWaylandExtensions() Q_DECL_NOEXCEPT;

    QString baseDir() const;
    QVariantMap runtimeConfiguration() const;
    QByteArray securityToken() const;
    bool slowAnimations() const;
    void setSlowAnimations(bool slow);
    QVariantMap systemProperties() const;
    QStringList loggingRules() const;
    QVariant useAMConsoleLogger() const;
    QString dltLongMessageBehavior() const;

    QString p2pDBusName() const;
    QString notificationDBusName() const;

    QVariantMap openGLConfiguration() const;

    QString iconThemeName() const;
    QStringList iconThemeSearchPaths() const;

    QVariantMap windowProperties(QWindow *window) const;
    void setWindowProperty(QWindow *window, const QString &name, const QVariant &value);
    void clearWindowPropertyCache(QWindow *window);

    QString applicationId() const;
    void setApplicationId(const QString &applicationId);

signals:
    void windowPropertyChanged(QWindow *window, const QString &name, const QVariant &value);
    void slowAnimationsChanged(bool slow);

private:
    static LauncherMain *s_instance;
    Q_DISABLE_COPY(LauncherMain)

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
    QString m_iconThemeName;
    QStringList m_iconThemeSearchPaths;
    QVariant m_useAMConsoleLogger;
    QString m_dltLongMessageBehavior;
#if defined(QT_WAYLANDCLIENT_LIB)
    QScopedPointer<WaylandQtAMClientExtension> m_waylandExtension;
#endif
};

QT_END_NAMESPACE_AM
