/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>
#include <QDir>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusError>

#include "global.h"
#include "applicationmanager_interface.h"
#include "applicationinstaller_interface.h"


int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(qSL("ApplicationManager Controller"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    QCoreApplication a(argc, argv);
    QCommandLineParser clp;

    clp.addHelpOption();
    clp.addVersionOption();
    clp.addPositionalArgument(qSL("package"), qSL("path to the package that should be installed"));

    clp.process(QCoreApplication::arguments());
    if (clp.positionalArguments().size() != 1)
        clp.showHelp(1);

    QString package = clp.positionalArguments().at(0);
    if (package.isEmpty())
        clp.showHelp(1);

    if (package == qL1S("-")) { // sent via stdin
        bool success = false;

        QTemporaryFile *tf = new QTemporaryFile(qApp);
        QFile in;

        if (tf->open() && in.open(stdin, QIODevice::ReadOnly)) {
            package = tf->fileName();

            while (!in.atEnd() && !tf->error())
                tf->write(in.read(1024 * 1024 * 8));

            success = in.atEnd() && !tf->error();
            tf->flush();
        }

        if (!success) {
            fprintf(stderr, "ERROR: Could not copy from stdin to temporary file %s\n", qPrintable(package));
            return 2;
        }
    }

    QFileInfo fi(package);
    if (!fi.exists() || !fi.isReadable() || !fi.isFile()) {
        fprintf(stderr, "ERROR: Package file is not readable: %s\n", qPrintable(package));
        return 3;
    }

    QDBusConnection conn = QDBusConnection(QString());

    QFile f(QDir::temp().absoluteFilePath(qSL("application-manager.dbus")));
    if (f.open(QFile::ReadOnly)) {
        QString dbus = QString::fromUtf8(f.readAll());
        if (dbus == qL1S("system"))
            conn = QDBusConnection::systemBus();
        else if (dbus.isEmpty())
            conn = QDBusConnection::sessionBus();
        else
            conn = QDBusConnection::connectToBus(dbus, qSL("custom"));
    }

    if (!conn.isConnected()) {
        fprintf(stderr, "ERROR: Could not connect to the session D-Bus: %s (%s)\n",
                qPrintable(conn.lastError().message()), qPrintable(conn.lastError().name()));
        return 5;
    }

    IoQtApplicationManagerInterface manager(qSL("io.qt.ApplicationManager"), qSL("/Manager"), conn);
    IoQtApplicationInstallerInterface installer(qSL("io.qt.ApplicationManager"), qSL("/Installer"), conn);

    fprintf(stdout, "Starting installation of package %s...\n", qPrintable(package));

    QString installationId;
    QString applicationId;

    // start the package installation

    QTimer::singleShot(0, [&installer, &package, &installationId]() {
        auto reply = installer.startPackageInstallation(qSL("internal-0"), package);
        reply.waitForFinished();
        if (reply.isError()) {
            fprintf(stderr, "ERROR: failed to call startPackageInstallation via DBus: %s\n", qPrintable(reply.error().message()));
            qApp->exit(6);
        }

        installationId = reply.value();
        if (installationId.isEmpty()) {
            fprintf(stderr, "ERROR: startPackageInstallation returned an empty taskId\n");
            qApp->exit(7);
        }
    });

    // as soon as we have the manifest available: get the app id and acknowledge the installation

    QObject::connect(&installer, &IoQtApplicationInstallerInterface::taskRequestingInstallationAcknowledge,
                     [&installer, &installationId, &applicationId](const QString &taskId, const QVariantMap &metadata) {
        if (taskId != installationId)
            return;
        applicationId = metadata.value(qSL("id")).toString();
        if (applicationId.isEmpty()) {
            fprintf(stderr, "ERROR: Could not find a valid application id in the package - got: %s\n", qPrintable(applicationId));
            qApp->exit(8);
        }
        fprintf(stdout, "Acknowledging package installation...\n");
        installer.acknowledgePackageInstallation(taskId);
    });

    // on failure: quit

    QObject::connect(&installer, &IoQtApplicationInstallerInterface::taskFailed,
                     [&installationId](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        fprintf(stderr, "ERROR: Failed to install package: %s (code: %d)\n", qPrintable(errorString), errorCode);
        qApp->exit(9);
    });

    // when the installation finished successfully: launch the application

    QObject::connect(&installer, &IoQtApplicationInstallerInterface::taskFinished,
                     [&manager, &applicationId, &installationId](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Installation finished - launching application...\n");
        auto reply = manager.startApplication(applicationId);
        reply.waitForFinished();
        if (reply.isError()) {
            fprintf(stderr, "ERROR: failed to call startApplication via DBus: %s\n", qPrintable(reply.error().message()));
            qApp->exit(10);
        }
        if (!reply.value()) {
            fprintf(stderr, "ERROR: startApplication failed\n");
            qApp->exit(11);
        }
        qApp->quit();
    });

    return a.exec();
}
