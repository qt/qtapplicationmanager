/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QTest>

static void detachLoopbacksAndUnmount(SudoClient *root, const QString &baseDir)
{
#ifdef Q_OS_LINUX
    if (baseDir.isEmpty()) {
        qCritical() << "detachLoopbacksAndUnmount() was called with an empty baseDir";
        return;
    }

    QMap<QString, QString> mounts = mountedDirectories();
    foreach (const QString &mountPoint, mounts.keys()) {
        if (mountPoint.startsWith(baseDir)) {
            bool success;
            if (root) {
                success = root->unmount(mountPoint, true);
            } else {
                QProcess p;
                p.start("/bin/umount", { "-lf", mountPoint });
                success = p.waitForStarted(3000) && p.waitForFinished(3000) && (p.exitCode() == 0);
            }
            if (!success)
                qWarning() << "ERROR: could not unmount" << mountPoint;
        }
    }

    QTest::qSleep(200);

    // we cannot directly open /dev/loop* devices here, since that would
    // require root privileges. The information about the backing-files
    // is however also available via sysfs for non-root users.
    QStringList loopbacks = QDir("/sys/block").entryList({ "loop*" }, QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &loopback, loopbacks) {
        QFile backing(QString::fromLatin1("/sys/block/%1/loop/backing_file").arg(loopback));

        if (backing.open(QFile::ReadOnly)) {
            QByteArray backingFile = backing.readAll().trimmed();
            if (backingFile.endsWith(" (deleted)"))
                backingFile.chop(10);
            backing.close();

            if (QString::fromLocal8Bit(backingFile).startsWith(baseDir)) {
                bool success;
                if (root) {
                    success = root->detachLoopback("/dev/" + loopback);
                } else {
                    QProcess p;
                    p.start("/sbin/losetup", { "-d", "/dev/" + loopback });
                    success = p.waitForStarted(3000) && p.waitForFinished(3000) && (p.exitCode() == 0);
                }
                if (!success)
                    qWarning() << "ERROR: could not detach loopback device" << ("/dev/" + loopback) << "from" << backing.fileName();
            }
        }
    }
    QTest::qSleep(200);
#else
    Q_UNUSED(baseDir)
    Q_UNUSED(root)
#endif // Q_OS_LINUX
}
