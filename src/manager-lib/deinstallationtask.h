/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

#include <QtAppManManager/asynchronoustask.h>

QT_BEGIN_NAMESPACE_AM

class Package;
class InstallationLocation;

class DeinstallationTask : public AsynchronousTask
{
    Q_OBJECT

public:
    DeinstallationTask(const QString &packageId, const QString &installationPath, const QString &documentPath,
                       bool forceDeinstallation, bool keepDocuments, QObject *parent = nullptr);

    bool cancel() override;

protected:
    void execute() override;

private:
    QString m_installationPath;
    QString m_documentPath;
    bool m_keepDocuments;
    bool m_canBeCanceled = true;
    bool m_canceled = false;
};

QT_END_NAMESPACE_AM
