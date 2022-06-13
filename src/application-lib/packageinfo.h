/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QDir>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

class tst_Application;

QT_BEGIN_NAMESPACE_AM

class InstallationReport;
class IntentInfo;
class ApplicationInfo;
class YamlPackageScanner;

class PackageInfo
{
public:
    ~PackageInfo();

    static const quint32 DataStreamVersion;

    void validate() const Q_DECL_NOEXCEPT_EXPR(false);

    QString id() const;

    QMap<QString, QString> names() const;
    QString name(const QString &language) const;
    QMap<QString, QString> descriptions() const;
    QString description(const QString &language) const;
    QString icon() const;
    QStringList categories() const;

    bool isBuiltIn() const;
    void setBuiltIn(bool builtIn);
    QString version() const;
    uint uid() const { return m_uid; }

    const QDir &baseDir() const;
    void setBaseDir(const QDir &dir);

    QVector<ApplicationInfo *> applications() const;
    QVector<IntentInfo *> intents() const;

    const InstallationReport *installationReport() const;
    void setInstallationReport(InstallationReport *report);

    void writeToDataStream(QDataStream &ds) const;
    static PackageInfo *readFromDataStream(QDataStream &ds);

    static bool isValidApplicationId(const QString &appId, QString *errorString = nullptr);
    static bool isValidIcon(const QString &icon, QString *errorString = nullptr);

    QString manifestPath() const;

    static PackageInfo *fromManifest(const QString &manifestPath);

private:
    PackageInfo();

    QString m_manifestName;
    QString m_id;
    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QStringList m_categories;
    QString m_icon; // relative to info.json location
    QString m_version;
    bool m_builtIn = false; // system package - not removable
    uint m_uid = uint(-1); // unix user id - move to installationReport
    QVector<ApplicationInfo *> m_applications;
    QVector<IntentInfo *> m_intents;

    // added by installer
    QScopedPointer<InstallationReport> m_installationReport;
    QDir m_baseDir;

    friend class YamlPackageScanner;
    friend class InstallationTask;
    Q_DISABLE_COPY(PackageInfo)
};

QT_END_NAMESPACE_AM
