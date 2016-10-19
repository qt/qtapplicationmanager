/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include "application.h"
#include "applicationdatabase.h"

QT_BEGIN_NAMESPACE_AM

class ApplicationDatabasePrivate
{
public:
    QFile *file;

    ApplicationDatabasePrivate()
        : file(0)
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

QVector<const Application *> ApplicationDatabase::read() throw (Exception)
{
    QVector<const Application *> apps;

    if (d->file->seek(0)) {
        QDataStream ds(d->file);

        forever {
            Application *app = Application::readFromDataStream(ds, apps);

            if (ds.status() != QDataStream::Ok) {
                if (ds.status() != QDataStream::ReadPastEnd) {
                    qDeleteAll(apps);
                    throw Exception(Error::System, "could not read from application database %1").arg(d->file->fileName());
                }
                break;
            }
            apps << app;
        }
    }

    return apps;
}

void ApplicationDatabase::write(const QVector<const Application *> &apps) throw (Exception)
{
    if (!d->file->seek(0))
        throw Exception(*d->file, "could not not seek to position 0 in the application database");
    if (!d->file->resize(0))
        throw Exception(*d->file, "could not truncate the application database");

    QDataStream ds(d->file);
    foreach (const Application *app, apps)
        app->writeToDataStream(ds, apps);
    if (ds.status() != QDataStream::Ok)
        throw Exception(*d->file, "could not write to application database");
}

QT_END_NAMESPACE_AM
