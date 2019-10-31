/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>

#include <stdio.h>

#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManPackage/packageutilities.h>
#include "packagingjob.h"

QT_USE_NAMESPACE_AM

enum Command {
    NoCommand,
    CreatePackage,
    DevSignPackage,
    DevVerifyPackage,
    StoreSignPackage,
    StoreVerifyPackage,
    YamlToJson
};

// REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
static struct {
    Command command;
    const char *name;
    const char *description;
} commandTable[] = {
    { CreatePackage,      "create-package",       "Create a new package." },
    { DevSignPackage,     "dev-sign-package",     "Add developer signature to package." },
    { DevVerifyPackage,   "dev-verify-package",   "Verify developer signature on package." },
    { StoreSignPackage,   "store-sign-package",   "Add store signature to package." },
    { StoreVerifyPackage, "store-verify-package", "Verify store signature on package." },
    { YamlToJson,         "yaml-to-json",         "Convenience functionality for build systems (internal)." }
};

static Command command(QCommandLineParser &clp)
{
    if (!clp.positionalArguments().isEmpty()) {
        QByteArray cmd = clp.positionalArguments().at(0).toLatin1();

        for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
            if (cmd == commandTable[i].name) {
                clp.clearPositionalArguments();
                clp.addPositionalArgument(qL1S(cmd), qL1S(commandTable[i].description), qL1S(cmd));
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
}

int main(int argc, char *argv[])
{
    PackageUtilities::ensureCorrectLocale();

    QCoreApplication::setApplicationName(qSL("Qt ApplicationManager Packager"));
    QCoreApplication::setOrganizationName(qSL("QtProject"));
    QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    QCoreApplication a(argc, argv);

    if (!PackageUtilities::checkCorrectLocale()) {
        fprintf(stderr, "ERROR: the packager needs a UTF-8 locale to work correctly:\n"
                        "       even automatically switching to C.UTF-8 or en_US.UTF-8 failed.\n");
        exit(2);
    }

    QByteArray desc = "\n\nAvailable commands are:\n";
    uint longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += "  ";
        desc += commandTable[i].name;
        desc += QByteArray(1 + int(longestName - qstrlen(commandTable[i].name)), ' ');
        desc += commandTable[i].description;
        desc += '\n';
    }

    desc += "\nMore information about each command can be obtained by running\n" \
            "  appman-packager <command> --help";

    QCommandLineParser clp;
    clp.setApplicationDescription(qSL("\n") + QCoreApplication::applicationName() + qL1S(desc));

    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(qSL("command"), qSL("The command to execute."));

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    try {
        PackagingJob *p = nullptr;

        // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
        switch (command(clp)) {
        default:
        case NoCommand:
            if (clp.isSet(qSL("version")))
                clp.showVersion();
            if (clp.isSet(qSL("help")))
                clp.showHelp();
            clp.showHelp(1);
            break;

        case CreatePackage: {
            clp.addOption({ qSL("verbose"), qSL("Dump the package's meta-data header and footer information to stdout.") });
            clp.addOption({ qSL("json"),    qSL("Output in JSON format instead of YAML.") });
            clp.addOption({{ qSL("extra-metadata"),      qSL("m") }, qSL("Add extra meta-data to the package, supplied on the commandline."), qSL("yaml-snippet") });
            clp.addOption({{ qSL("extra-metadata-file"), qSL("M") }, qSL("Add extra meta-data to the package, read from file."), qSL("yaml-file") });
            clp.addOption({{ qSL("extra-signed-metadata"),      qSL("s") }, qSL("Add extra, digitally signed, meta-data to the package, supplied on the commandline."), qSL("yaml-snippet") });
            clp.addOption({{ qSL("extra-signed-metadata-file"), qSL("S") }, qSL("Add extra, digitally signed, meta-data to the package, read from file."), qSL("yaml-file") });
            clp.addPositionalArgument(qSL("package"),          qSL("The file name of the created package."));
            clp.addPositionalArgument(qSL("source-directory"), qSL("The package's content root directory."));
            clp.process(a);

            if (clp.positionalArguments().size() != 3)
                clp.showHelp(1);

            auto parseYamlMetada = [](const QStringList &metadataSnippets, const QStringList &metadataFiles, bool isSigned) -> QVariantMap {
                QVariantMap result;
                QVector<QPair<QByteArray, QString>> metadata;

                for (const QString &file : metadataFiles) {
                    QFile f(file);
                    if (!f.open(QIODevice::ReadOnly))
                        throw Exception(f, "Could not open metadata file for reading");
                    metadata.append(qMakePair(f.readAll(), file));
                }
                for (const QString &snippet : metadataSnippets)
                    metadata.append(qMakePair(snippet.toUtf8(), QString()));

                for (const auto &md : metadata) {
                    QVector<QVariant> docs;
                    try {
                        docs = YamlParser::parseAllDocuments(md.first);
                    } catch (const Exception &e) {
                        throw Exception(Error::IO, "in --extra-%1metadata%2 %3: %4")
                                .arg(isSigned ? "signed-" : "").arg(md.second.isEmpty() ? "": "-file")
                                .arg(md.second.isEmpty() ? qSL("option") : md.second)
                                .arg(e.errorString());
                    }
                    if (docs.size() < 1) {
                        throw Exception("Could not parse --extra-%1metadata%2 %3: Invalid document format")
                                .arg(isSigned ? "signed-" : "").arg(md.second.isEmpty() ? "": "-file")
                                .arg(md.second.isEmpty() ? qSL("option") : md.second);
                    }
                    for (auto doc : docs)
                        recursiveMergeVariantMap(result, doc.toMap());
                }
                return result;
            };

            QVariantMap extraMetaDataMap = parseYamlMetada(clp.values(qSL("extra-metadata")),
                                                           clp.values(qSL("extra-metadata-file")),
                                                           false);
            QVariantMap extraSignedMetaDataMap = parseYamlMetada(clp.values(qSL("extra-signed-metadata")),
                                                                 clp.values(qSL("extra-signed-metadata-file")),
                                                                 true);

            p = PackagingJob::create(clp.positionalArguments().at(1),
                                     clp.positionalArguments().at(2),
                                     extraMetaDataMap,
                                     extraSignedMetaDataMap,
                                     clp.isSet(qSL("json")));
            break;
        }
        case DevSignPackage:
            clp.addOption({ qSL("verbose"), qSL("Dump the package's meta-data header and footer information to stdout.") });
            clp.addOption({ qSL("json"),    qSL("Output in JSON format instead of YAML.") });
            clp.addPositionalArgument(qSL("package"),        qSL("File name of the unsigned package (input)."));
            clp.addPositionalArgument(qSL("signed-package"), qSL("File name of the signed package (output)."));
            clp.addPositionalArgument(qSL("certificate"),    qSL("PKCS#12 certificate file."));
            clp.addPositionalArgument(qSL("password"),       qSL("Password for the PKCS#12 certificate."));
            clp.process(a);

            if (clp.positionalArguments().size() != 5)
                clp.showHelp(1);

            p = PackagingJob::developerSign(clp.positionalArguments().at(1),
                                            clp.positionalArguments().at(2),
                                            clp.positionalArguments().at(3),
                                            clp.positionalArguments().at(4),
                                            clp.isSet(qSL("json")));
            break;

        case DevVerifyPackage:
            clp.addOption({ qSL("verbose"), qSL("Print details regarding the verification to stdout.") });
            clp.addPositionalArgument(qSL("package"),      qSL("File name of the signed package (input)."));
            clp.addPositionalArgument(qSL("certificates"), qSL("The developer's CA certificate file(s)."), qSL("certificates..."));
            clp.process(a);

            if (clp.positionalArguments().size() < 3)
                clp.showHelp(1);

            p = PackagingJob::developerVerify(clp.positionalArguments().at(1),
                                              clp.positionalArguments().mid(2));
            break;

        case StoreSignPackage:
            clp.addOption({ qSL("verbose"), qSL("Dump the package's meta-data header and footer information to stdout.") });
            clp.addOption({ qSL("json"),    qSL("Output in JSON format instead of YAML.") });
            clp.addPositionalArgument(qSL("package"),        qSL("File name of the unsigned package (input)."));
            clp.addPositionalArgument(qSL("signed-package"), qSL("File name of the signed package (output)."));
            clp.addPositionalArgument(qSL("certificate"),    qSL("PKCS#12 certificate file."));
            clp.addPositionalArgument(qSL("password"),       qSL("Password for the PKCS#12 certificate."));
            clp.addPositionalArgument(qSL("hardware-id"),    qSL("Unique hardware id to which this package gets bound."));
            clp.process(a);

            if (clp.positionalArguments().size() != 6)
                clp.showHelp(1);

            p = PackagingJob::storeSign(clp.positionalArguments().at(1),
                                        clp.positionalArguments().at(2),
                                        clp.positionalArguments().at(3),
                                        clp.positionalArguments().at(4),
                                        clp.positionalArguments().at(5),
                                        clp.isSet(qSL("json")));
            break;

        case StoreVerifyPackage:
            clp.addOption({ qSL("verbose"), qSL("Print details regarding the verification to stdout.") });
            clp.addPositionalArgument(qSL("package"),      qSL("File name of the signed package (input)."));
            clp.addPositionalArgument(qSL("certificates"), qSL("Store CA certificate file(s)."), qSL("certificates..."));
            clp.addPositionalArgument(qSL("hardware-id"),  qSL("Unique hardware id to which this package was bound."));
            clp.process(a);

            if (clp.positionalArguments().size() < 4)
                clp.showHelp(1);

            p = PackagingJob::storeVerify(clp.positionalArguments().at(1),
                                          clp.positionalArguments().mid(2, clp.positionalArguments().size() - 3),
                                          *--clp.positionalArguments().cend());
            break;

        case YamlToJson: {
            clp.addOption({{ qSL("i"), qSL("document-index") }, qSL("Only output the specified YAML sub-document."), qSL("index") });
            clp.addPositionalArgument(qSL("yaml-file"), qSL("YAML file name, defaults to stdin (input)."));
            clp.process(a);

            if (clp.positionalArguments().size() > 2)
                clp.showHelp(1);

            QString yamlName = (clp.positionalArguments().size() == 2) ? clp.positionalArguments().at(1)
                                                                       : qSL("-");
            QFile yaml(yamlName);
            if (yamlName == qL1S("-")) {
                if (!yaml.open(0, QIODevice::ReadOnly))
                    throw Exception("Could not open stdin for reading");
            } else if (!yaml.open(QIODevice::ReadOnly)) {
                throw Exception(yaml, "Could not open YAML input file");
            }

            QVector<QVariant> docs = YamlParser::parseAllDocuments(yaml.readAll());
            QJsonDocument json;
            if (clp.isSet(qSL("i"))) {
                bool isInt;
                int index = clp.value(qSL("i")).toInt(&isInt);
                if (!isInt || index < 0) {
                    throw Exception("Invalid document index specified: %1").arg(clp.value(qSL("i")));
                } else if (index >= docs.size()) {
                    throw Exception("Requested YAML sub document at index %1, but only indices 0 to %2 are available in this document.")
                            .arg(index).arg(docs.size() - 1);
                } else {
                    json = QJsonDocument::fromVariant(docs.at(index));
                }
            } else {
                json.setArray(QJsonArray::fromVariantList(docs.toList()));
            }
            fprintf(stdout, "%s", json.toJson(QJsonDocument::Indented).constData());
            return 0;
        }
        }

        if (!p)
            return 2;

        p->execute();
        if (clp.isSet(qSL("verbose")) && !p->output().isEmpty())
            fprintf(stdout, "%s\n", qPrintable(p->output()));
        return p->resultCode();
    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
}
