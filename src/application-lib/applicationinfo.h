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

#include <QDataStream>
#include <QDir>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <QtAppManCommon/global.h>
#include <QtAppManApplication/installationreport.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationManager;
class InstallationReport;

class AbstractApplicationInfo
{
public:
    AbstractApplicationInfo();
    virtual ~AbstractApplicationInfo() {}

    QString id() const;
    int uniqueNumber() const;
    QMap<QString, QString> names() const;
    QString name(const QString &language) const;
    QString icon() const;
    QString documentUrl() const;
    QVariantMap applicationProperties() const;
    QVariantMap allAppProperties() const;

    QVariantMap toVariantMap() const;
    virtual void toVariantMapHelper(QVariantMap &map) const;

    virtual bool isAlias() const = 0;
    virtual void writeToDataStream(QDataStream &ds) const;
    virtual void validate() const Q_DECL_NOEXCEPT_EXPR(false);

    static bool isValidApplicationId(const QString &appId, bool isAliasName = false, QString *errorString = nullptr);
    static AbstractApplicationInfo *readFromDataStream(QDataStream &ds);

protected:
    virtual void read(QDataStream &ds);

    // static part from info.json
    QString m_id;
    int m_uniqueNumber;

    QMap<QString, QString> m_name; // language -> name
    QString m_icon; // relative to info.json location
    QString m_documentUrl;
    QVariantMap m_sysAppProperties;
    QVariantMap m_allAppProperties;

    friend class YamlApplicationScanner;
};

class ApplicationAliasInfo : public AbstractApplicationInfo
{
public:
    bool isAlias() const override { return true; }
};

class ApplicationInfo : public AbstractApplicationInfo
{
public:
    ApplicationInfo();

    bool isAlias() const override { return false; }
    void writeToDataStream(QDataStream &ds) const override;
    void validate() const Q_DECL_NOEXCEPT_EXPR(false) override;

    const QDir &codeDir() const { return m_codeDir; }
    QString absoluteCodeFilePath() const;
    QString codeFilePath() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QVariantMap environmentVariables() const { return m_environmentVariables; }
    bool isBuiltIn() const;
    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QStringList categories() const;
    QString version() const;
    QVariantMap openGLConfiguration() const;
    bool supportsApplicationInterface() const;

    void setSupportsApplicationInterface(bool supportsAppInterface);
    void setBuiltIn(bool builtIn);

    const InstallationReport *installationReport() const { return m_installationReport.data(); }
    void setInstallationReport(InstallationReport *report) { m_installationReport.reset(report); }
    QString manifestDir() const { return m_manifestDir.absolutePath(); }
    uint uid() const { return m_uid; }
    void setManifestDir(const QString &path) { m_manifestDir = path; }
    void setCodeDir(const QString &path) { m_codeDir = path; }

    void toVariantMapHelper(QVariantMap &map) const override;

private:
    void read(QDataStream &ds) override;

    QString m_codeFilePath; // relative to info.json location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    QVariantMap m_environmentVariables;
    bool m_supportsApplicationInterface = false;

    bool m_builtIn = false; // system app - not removable

    QStringList m_capabilities;
    QStringList m_categories;
    QStringList m_mimeTypes;

    QString m_version;

    QVariantMap m_openGLConfiguration;

    // added by installer
    QScopedPointer<InstallationReport> m_installationReport;
    QDir m_manifestDir;
    QDir m_codeDir;
    uint m_uid = uint(-1); // unix user id - move to installationReport

    friend class YamlApplicationScanner;
    friend class ApplicationManager; // needed to update installation status
    friend class ApplicationDatabase; // needed to create ApplicationInfo objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    Q_DISABLE_COPY(ApplicationInfo)
};

QT_END_NAMESPACE_AM
