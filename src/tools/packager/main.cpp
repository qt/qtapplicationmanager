/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
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
        QByteArray cmd = clp.positionalArguments().at(0).toLatin1();

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

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(qSL("ApplicationManager Packager"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    QCoreApplication a(argc, argv);

    QString desc = qSL("\nPelagicore ApplicationManager packaging tool\n\nAvailable commands are:\n");
    uint longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += qSL("  %1%2  %3\n")
                .arg(qL1S(commandTable[i].name),
                     QString(longestName - qstrlen(commandTable[i].name), qL1C(' ')),
                     qL1S(commandTable[i].description));
    }

    desc += qSL("\nMore information about each command can be obtained by running\n  application-packager <command> --help");

    QCommandLineParser clp;
    clp.setApplicationDescription(desc);

    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(qSL("command"), qSL("The command to execute."));

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }

    Packager *p = 0;

    switch (command(clp)) {
    default:
    case NoCommand:
        if (clp.isSet(qSL("version"))) {
#if QT_VERSION < QT_VERSION_CHECK(5,4,0)
            fprintf(stdout, "%s %s\n", qPrintable(QCoreApplication::applicationName()), qPrintable(QCoreApplication::applicationVersion()));
            exit(0);
#else
            clp.showVersion();
#endif
        }
        if (clp.isSet(qSL("help")))
            clp.showHelp();
        clp.showHelp(1);
        break;

    case CreatePackage:
        clp.addPositionalArgument(qSL("package"),          qSL("The file name of the created package."));
        clp.addPositionalArgument(qSL("source-directory"), qSL("The package's content root directory."));
        clp.process(a);

        if (clp.positionalArguments().size() != 3)
            clp.showHelp(1);

        p = Packager::create(clp.positionalArguments().at(1),
                             clp.positionalArguments().at(2));
        break;

    case DevSignPackage:
        clp.addPositionalArgument(qSL("package"),        qSL("File name of the unsigned package (input)."));
        clp.addPositionalArgument(qSL("signed-package"), qSL("File name of the signed package (output)."));
        clp.addPositionalArgument(qSL("certificate"),    qSL("PKCS#12 certificate file."));
        clp.addPositionalArgument(qSL("password"),       qSL("Password for the PKCS#12 certificate."));
        clp.process(a);

        if (clp.positionalArguments().size() != 5)
            clp.showHelp(1);

        p = Packager::developerSign(clp.positionalArguments().at(1),
                                    clp.positionalArguments().at(2),
                                    clp.positionalArguments().at(3),
                                    clp.positionalArguments().at(4));
        break;

    case DevVerifyPackage:
        clp.addPositionalArgument(qSL("package"),      qSL("File name of the signed package (input)."));
        clp.addPositionalArgument(qSL("certificates"), qSL("The developer's CA certificate file(s)."), qSL("certificates..."));
        clp.process(a);

        if (clp.positionalArguments().size() < 3)
            clp.showHelp(1);

        p = Packager::developerVerify(clp.positionalArguments().at(1),
                                      clp.positionalArguments().mid(2));
        break;

    case StoreSignPackage:
        clp.addPositionalArgument(qSL("package"),        qSL("File name of the unsigned package (input)."));
        clp.addPositionalArgument(qSL("signed-package"), qSL("File name of the signed package (output)."));
        clp.addPositionalArgument(qSL("certificate"),    qSL("PKCS#12 certificate file."));
        clp.addPositionalArgument(qSL("password"),       qSL("Password for the PKCS#12 certificate."));
        clp.addPositionalArgument(qSL("hardware-id"),    qSL("Unique hardware id to which this package gets bound."));
        clp.process(a);

        if (clp.positionalArguments().size() != 6)
            clp.showHelp(1);

        p = Packager::storeSign(clp.positionalArguments().at(1),
                                clp.positionalArguments().at(2),
                                clp.positionalArguments().at(3),
                                clp.positionalArguments().at(4),
                                clp.positionalArguments().at(5));
        break;

    case StoreVerifyPackage:
        clp.addPositionalArgument(qSL("package"),      qSL("File name of the signed package (input)."));
        clp.addPositionalArgument(qSL("certificates"), qSL("Store CA certificate file(s)."), qSL("certificates..."));
        clp.addPositionalArgument(qSL("hardware-id"),  qSL("Unique hardware id to which this package was bound."));
        clp.process(a);

        if (clp.positionalArguments().size() < 4)
            clp.showHelp(1);

        p = Packager::storeVerify(clp.positionalArguments().at(1),
                                  clp.positionalArguments().mid(2, clp.positionalArguments().size() - 2),
                                  *--clp.positionalArguments().cend());
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
