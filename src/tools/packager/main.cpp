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
#include <QStringList>
#include <QDebug>

#include <stdio.h>

#include "utilities.h"
#include "packager.h"


enum Command {
    NoCommand,
    CreatePackage,
    DevSignPackage,
    DevVerifyPackage,
    StoreSignPackage,
    StoreVerifyPackage,
};

static struct {
    Command command;
    const char *name;
    const char *description;
} commandTable[] = {
    { CreatePackage,      "create-package",       "Create a new package." },
    { DevSignPackage,     "dev-sign-package",     "Add developer signature to package." },
    { DevVerifyPackage,   "dev-verify-package",   "Verify developer signature on package." },
    { StoreSignPackage,   "store-sign-package",   "Add store signature to package." },
    { StoreVerifyPackage, "store-verify-package", "Verify store signature on package." }
};

static Command command(QCommandLineParser &clp)
{
    if (!clp.positionalArguments().isEmpty()) {
        QByteArray cmd = clp.positionalArguments().first().toLatin1();

        for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
            if (cmd == commandTable[i].name) {
                clp.clearPositionalArguments();
                clp.addPositionalArgument(cmd, commandTable[i].description, cmd);
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
}

int main(int argc, char **argv)
{
    QCoreApplication::setApplicationName("ApplicationManager Packager");
    QCoreApplication::setOrganizationName(QLatin1String("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(QLatin1String("pelagicore.com"));
    QCoreApplication::setApplicationVersion(AM_GIT_VERSION);

    QCoreApplication app(argc, argv);

    QString desc = "\nPelagicore ApplicationManager packaging tool\n\nAvailable commands are:\n";
    uint longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += QString::fromLatin1("  %1%2  %3\n")
                .arg(commandTable[i].name)
                .arg(QString(longestName - qstrlen(commandTable[i].name), ' '))
                .arg(commandTable[i].description);
    }

    desc += "\nMore information about each command can be obtained by running\n  application-packager <command> --help";

    QCommandLineParser clp;
    clp.setApplicationDescription(desc);

    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument("command", "The command to execute.");

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }

    Packager *p = 0;

    switch (command(clp)) {
    default:
    case NoCommand:
        if (clp.isSet("version")) {
#if QT_VERSION < QT_VERSION_CHECK(5,4,0)
            fprintf(stdout, "%s %s\n", qPrintable(QCoreApplication::applicationName()), qPrintable(QCoreApplication::applicationVersion()));
            exit(0);
#else
            clp.showVersion();
#endif
        }
        if (clp.isSet("help"))
            clp.showHelp();
        clp.showHelp(1);
        break;

    case CreatePackage:
        clp.addPositionalArgument("package",          "The file name of the created package.");
        clp.addPositionalArgument("source-directory", "The package's content root directory.");
        clp.process(app);

        if (clp.positionalArguments().size() != 3)
            clp.showHelp(1);

        p = Packager::create(clp.positionalArguments().at(1),
                             clp.positionalArguments().at(2));
        break;

    case DevSignPackage:
        clp.addPositionalArgument("package",        "File name of the unsigned package (input).");
        clp.addPositionalArgument("signed-package", "File name of the signed package (output).");
        clp.addPositionalArgument("certificate",    "PKCS#12 certificate file.");
        clp.addPositionalArgument("password",       "Password for the PKCS#12 certificate.");
        clp.process(app);

        if (clp.positionalArguments().size() != 5)
            clp.showHelp(1);

        p = Packager::developerSign(clp.positionalArguments().at(1),
                                    clp.positionalArguments().at(2),
                                    clp.positionalArguments().at(3),
                                    clp.positionalArguments().at(4));
        break;

    case DevVerifyPackage:
        clp.addPositionalArgument("package",      "File name of the signed package (input).");
        clp.addPositionalArgument("certificates", "The developer's CA certificate file(s).", "certificates...");
        clp.process(app);

        if (clp.positionalArguments().size() != 3)
            clp.showHelp(1);

        p = Packager::developerVerify(clp.positionalArguments().at(1),
                                      clp.positionalArguments().mid(2));
        break;

    case StoreSignPackage:
        clp.addPositionalArgument("package",        "File name of the unsigned package (input).");
        clp.addPositionalArgument("signed-package", "File name of the signed package (output).");
        clp.addPositionalArgument("certificate",    "PKCS#12 certificate file.");
        clp.addPositionalArgument("password",       "Password for the PKCS#12 certificate.");
        clp.addPositionalArgument("hardware-id",    "Unique hardware id to which this package gets bound.");
        clp.process(app);

        if (clp.positionalArguments().size() != 6)
            clp.showHelp(1);

        p = Packager::storeSign(clp.positionalArguments().at(1),
                                clp.positionalArguments().at(2),
                                clp.positionalArguments().at(3),
                                clp.positionalArguments().at(4),
                                clp.positionalArguments().at(5));
        break;

    case StoreVerifyPackage:
        clp.addPositionalArgument("package",     "File name of the signed package (input).");
        clp.addPositionalArgument("certificate", "Store CA certificate file(s).", "certificates...");
        clp.addPositionalArgument("hardware-id", "Unique hardware id to which this package was bound.");
        clp.process(app);

        if (clp.positionalArguments().size() != 4)
            clp.showHelp(1);

        p = Packager::storeVerify(clp.positionalArguments().at(1),
                                  clp.positionalArguments().mid(2, clp.positionalArguments().size() - 2),
                                  clp.positionalArguments().last());
        break;
    }

    if (!p)
        return 2;
    try {
        p->execute();
        if (!p->output().isEmpty())
            fprintf(stdout, "%s\n", qPrintable(p->output()));
        return p->resultCode();
    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
}
