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

#include <QFile>
#include <QDataStream>
#include <QTemporaryFile>
#include <QScopedPointer>

#include "application.h"
#include "applicationdatabase.h"
#include "exception.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

class ApplicationDatabasePrivate
{
public:
    Application *findAppWithId(const QString &id, const QVector<AbstractApplication *> &apps)
    {
        QString baseId = id.section(qL1C('@'), 0, 0);
        for (auto *otherApp : apps) {
            if (otherApp->id() == baseId) {
                Q_ASSERT(!otherApp->isAlias());
                return static_cast<Application*>(otherApp);
            }
        }
        return nullptr;
    }

    void validateWritableFile()
    {
        if (!file || !file->isOpen() || !file->isWritable())
            throw Exception("application database %1 is not opened for writing").arg(file ? file->fileName() : qSL("<null>"));
        if (!file->seek(0))
            throw Exception(*file, "could not not seek to position 0 in the application database");
        if (!file->resize(0))
            throw Exception(*file, "could not truncate the application database");
    }

    QFile *file = nullptr;

    ApplicationDatabasePrivate()
    { }
    ~ApplicationDatabasePrivate()
    { delete file; }
};

ApplicationDatabase::ApplicationDatabase(const QString &fileName)
    : d(new ApplicationDatabasePrivate())
{
    d->file = new QFile(fileName);
    d->file->open(QFile::ReadWrite);
}

ApplicationDatabase::ApplicationDatabase()
    : d(new ApplicationDatabasePrivate())
{
    d->file = new QTemporaryFile(qSL("am-apps-db"));
    d->file->open(QFile::ReadWrite);
}

ApplicationDatabase::~ApplicationDatabase()
{
    delete d;
}

bool ApplicationDatabase::isValid() const
{
    return d->file && d->file->isOpen();
}

bool ApplicationDatabase::isTemporary() const
{
    return qobject_cast<QTemporaryFile *>(d->file);
}

QString ApplicationDatabase::errorString() const
{
    return d->file->errorString();
}

QString ApplicationDatabase::name() const
{
    return d->file->fileName();
}

QVector<AbstractApplication *> ApplicationDatabase::read(AbstractApplicationManager *appMan) Q_DECL_NOEXCEPT_EXPR(false)
{
    if (!d->file || !d->file->isOpen() || !d->file->isReadable())
        throw Exception("application database %1 is not opened for reading").arg(d->file ? d->file->fileName() : qSL("<null>"));

    QVector<AbstractApplication *> apps;

    if (d->file->seek(0)) {
        QDataStream ds(d->file);

        forever {
            QScopedPointer<AbstractApplicationInfo> appInfo(AbstractApplicationInfo::readFromDataStream(ds));

            QScopedPointer<AbstractApplication> app;
            if (appInfo->isAlias()) {
                Application *originalApp = d->findAppWithId(appInfo->id(), apps);
                if (!originalApp)
                    throw Exception(Error::Parse, "Could not find base app for alias id %2").arg(appInfo->id());
                app.reset(new ApplicationAlias(originalApp, static_cast<ApplicationAliasInfo*>(appInfo.take()), appMan));
            } else {
                AbstractApplication *otherAbsApp = findAppWithId(apps, appInfo->id());
                if (otherAbsApp) {
                    // There's already another ApplicationInfo with the same id. It's probably an update for a
                    // built-in app, in which case we use the same Application instance to hold both
                    // ApplicationInfo instances.
                    bool merged = false;

                    if (!otherAbsApp->isAlias()) {
                        auto otherApp = static_cast<Application*>(otherAbsApp);
                        auto fullAppInfo = static_cast<ApplicationInfo*>(appInfo.data());
                        if (otherApp->isBuiltIn() && !fullAppInfo->isBuiltIn() && !otherApp->updatedInfo()) {
                            otherApp->setUpdatedInfo(static_cast<ApplicationInfo*>(appInfo.take()));
                            merged = true;
                        } else if (!otherApp->isBuiltIn() && fullAppInfo->isBuiltIn() && !otherApp->updatedInfo()) {
                            auto currentBaseInfo = otherApp->takeBaseInfo();
                            otherApp->setBaseInfo(static_cast<ApplicationInfo*>(appInfo.take()));
                            otherApp->setUpdatedInfo(currentBaseInfo);
                            merged = true;
                        }
                    }

                    if (!merged)
                        qCWarning(LogSystem).nospace() << "ApplicationDatabase: found a second application with id "
                            << appInfo->id() << " which is not an update for a built-in one. Ignoring it.";
                } else {
                    app.reset(new Application(static_cast<ApplicationInfo*>(appInfo.take()), appMan));
                }
            }

            if (ds.status() != QDataStream::Ok) {
                if (ds.status() != QDataStream::ReadPastEnd) {
                    qDeleteAll(apps);
                    throw Exception("could not read from application database %1").arg(d->file->fileName());
                }
                break;
            }

            if (!app.isNull())
                apps << app.take();
        }
    }

    return apps;
}

void ApplicationDatabase::write(const QVector<const AbstractApplicationInfo *> &apps) Q_DECL_NOEXCEPT_EXPR(false)
{
    d->validateWritableFile();

    QDataStream ds(d->file);
    for (auto *app : apps)
        app->writeToDataStream(ds);

    if (ds.status() != QDataStream::Ok)
        throw Exception(*d->file, "could not write to application database");
}

void ApplicationDatabase::write(const QVector<AbstractApplication *> &apps) Q_DECL_NOEXCEPT_EXPR(false)
{
    d->validateWritableFile();

    QDataStream ds(d->file);
    for (auto *app : apps) {
        app->info()->writeToDataStream(ds);
        if (!app->isAlias()) {
            auto fullApp = static_cast<Application*>(app);
            if (fullApp->updatedInfo())
                fullApp->updatedInfo()->writeToDataStream(ds);
        }
    }

    if (ds.status() != QDataStream::Ok)
        throw Exception(*d->file, "could not write to application database");
}

void ApplicationDatabase::invalidate()
{
    if (d->file) {
        if (d->file->isOpen())
            d->file->close();
        d->file->remove();
        d->file = nullptr;
    }
}


AbstractApplication *ApplicationDatabase::findAppWithId(QVector<AbstractApplication *> &apps, const QString &id)
{
    for (AbstractApplication *app : apps) {
        if (app->id() == id)
            return app;
    }
    return nullptr;
}

QT_END_NAMESPACE_AM
