/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QUrl>
#include <QStringList>
#include <QWaitCondition>
#include <QMutex>

#include "applicationinstaller.h"
#include "installationreport.h"
#include "asynchronoustask.h"
#include "scopeutilities.h"
#include "exception.h"

class Application;
class PackageExtractor;


class InstallationTask : public AsynchronousTask
{
    Q_OBJECT
public:
    InstallationTask(const InstallationLocation &installationLocation, const QUrl &sourceUrl, QObject *parent = 0);
    ~InstallationTask();

    void acknowledge();
    bool cancel() override;

signals:
    void finishedPackageExtraction();

protected:
    void execute() override;

private:
    void startInstallation() throw(Exception);
    void finishInstallation() throw(Exception);
    void checkExtractedFile(const QString &file) throw(Exception);

private:
    ApplicationInstaller *m_ai;
    const InstallationLocation &m_installationLocation;
    QUrl m_sourceUrl;
    bool m_foundInfo = false;
    bool m_foundIcon = false;
    bool m_locked = false;
    uint m_extractedFileCount = 0;
    bool m_managerApproval = false;
    Application *m_app = nullptr;

    // changes to these 4 member variables are protected by m_mutex
    PackageExtractor *m_extractor = nullptr;
    bool m_canceled = false;
    bool m_installationAcknowledged = false;
    QWaitCondition m_installationAcknowledgeWaitCondition;

    QDir m_manifestDir;
    QDir m_applicationDir;
    QDir m_extractionDir;
    QString m_applicationImageFile;
    QString m_extractionImageFile;

    ScopedDirectoryCreator m_manifestDirPlusCreator;
    ScopedDirectoryCreator m_installationDirCreator;
    ScopedFileCreator m_imageCreator;
    ScopedDirectoryCreator m_tmpMountPointCreator;
    ScopedLoopbackCreator m_loopbackCreator;
    ScopedMounter m_imageMounter;
};
