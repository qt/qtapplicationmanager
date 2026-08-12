// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DEINSTALLATIONTASK_H
#define DEINSTALLATIONTASK_H

#include <QtAppManSystemUI/qtappmansystemuiglobal.h>
#include <QtAppManSystemUI/asynchronoustask.h>

QT_BEGIN_NAMESPACE_AM

class Package;

class Q_APPMANSYSTEMUI_EXPORT DeinstallationTask : public AsynchronousTask
{
    Q_OBJECT

public:
    DeinstallationTask(const QString &packageId, const QString &installationPath, const QString &documentPath,
                       bool forceDeinstallation, bool keepDocuments, Origin origin, QObject *parent = nullptr);

    bool cancel() override;

protected:
    void execute() override;

private:
    QString m_installationPath;
    QString m_documentPath;
    bool m_keepDocuments;
    bool m_canBeCanceled = true;
};

QT_END_NAMESPACE_AM

#endif // DEINSTALLATIONTASK_H
