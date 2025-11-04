// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef PSPACKAGES_H
#define PSPACKAGES_H

#include <memory>
#include <QObject>
#include <QtAppManApplication/packageinfo.h>


class PSConfiguration;
class PSPackagesPrivate;


class PSPackage
{
public:
    QString id;
    QByteArray iconData;
    QString filePath;
    QString architecture;
    QByteArray sha1;
    std::unique_ptr<QtAM::PackageInfo> packageInfo;

    QString architectureOrAll() const;
};


class PSPackages : public QObject
{
    Q_OBJECT

public:
    PSPackages(PSConfiguration *cfg, QObject *parent = nullptr);
    ~PSPackages() override;

    void initialize();

    QStringList categories() const;

    PSPackage *scan(const QString &filePath);
    QList<PSPackage *> byArchitecture(const QString &architecture) const;
    PSPackage *byIdAndArchitecture(const QString &id, const QString &architecture) const;

    enum class UploadResult {
        Added,
        Updated,
        NoChanges
    };

    std::pair<UploadResult, PSPackage *> upload(const QString &filePath);
    void storeSign(PSPackage *sp, const QString &hardwareId, QIODevice *destination);
    int removeIf(const std::function<bool (PSPackage *)> &pred);

private:
    std::unique_ptr<PSPackagesPrivate> d;
};

#endif // PSPACKAGES_H
