// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <memory>
#include <stdio.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>

#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManCrypto/cryptography.h>
#include <QtAppManPackage/packageutilities.h>
#include "packagingjob.h"

using namespace Qt::StringLiterals;


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
                clp.addPositionalArgument(QString::fromLatin1(cmd),
                                          QString::fromLatin1(commandTable[i].description),
                                          QString::fromLatin1(cmd));
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
}

int main(int argc, char *argv[])
{
    // enable OpenSSL3 to load old certificates
    Cryptography::enableOpenSsl3LegacyProvider();

    QCoreApplication::setApplicationName(u"Qt ApplicationManager Packager"_s);
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(QString::fromLatin1(QT_AM_VERSION_STR));

    QCoreApplication a(argc, argv);

    QByteArray desc = "\n\nAvailable commands are:\n";
    size_t longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += "  ";
        desc += commandTable[i].name;
        desc += QByteArray(1 + qsizetype(longestName - qstrlen(commandTable[i].name)), ' ');
        desc += commandTable[i].description;
        desc += '\n';
    }

    desc += "\nMore information about each command can be obtained by running\n" \
            "  appman-packager <command> --help";

    QCommandLineParser clp;
    clp.setApplicationDescription(u"\n"_s + QCoreApplication::applicationName() + QString::fromLatin1(desc));

    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(u"command"_s, u"The command to execute."_s);

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    try {
        std::unique_ptr<PackagingJob> p;

        // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
        switch (command(clp)) {
        default:
        case NoCommand:
            if (clp.isSet(u"version"_s))
                clp.showVersion();
            if (clp.isSet(u"help"_s))
                clp.showHelp();
            clp.showHelp(1);
            break;

        case CreatePackage: {
            clp.addOption({ u"verbose"_s, u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,    u"Output in JSON format instead of YAML."_s });
            clp.addOption({{ u"extra-metadata"_s,      u"m"_s }, u"Add extra meta-data to the package, supplied on the command line."_s, u"yaml-snippet"_s });
            clp.addOption({{ u"extra-metadata-file"_s, u"M"_s }, u"Add extra meta-data to the package, read from file."_s, u"yaml-file"_s });
            clp.addOption({{ u"extra-signed-metadata"_s,      u"s"_s }, u"Add extra, digitally signed, meta-data to the package, supplied on the command line."_s, u"yaml-snippet"_s });
            clp.addOption({{ u"extra-signed-metadata-file"_s, u"S"_s }, u"Add extra, digitally signed, meta-data to the package, read from file."_s, u"yaml-file"_s });
            clp.addPositionalArgument(u"package"_s,          u"The file name of the created package."_s);
            clp.addPositionalArgument(u"source-directory"_s, u"The package's content root directory."_s);
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
                                .arg(md.second.isEmpty() ? u"option"_s : md.second)
                                .arg(e.errorString());
                    }
                    if (docs.size() < 1) {
                        throw Exception("Could not parse --extra-%1metadata%2 %3: Invalid document format")
                                .arg(isSigned ? "signed-" : "").arg(md.second.isEmpty() ? "": "-file")
                                .arg(md.second.isEmpty() ? u"option"_s : md.second);
                    }
                    for (const auto &doc : docs)
                        recursiveMergeVariantMap(result, doc.toMap());
                }
                return result;
            };

            QVariantMap extraMetaDataMap = parseYamlMetada(clp.values(u"extra-metadata"_s),
                                                           clp.values(u"extra-metadata-file"_s),
                                                           false);
            QVariantMap extraSignedMetaDataMap = parseYamlMetada(clp.values(u"extra-signed-metadata"_s),
                                                                 clp.values(u"extra-signed-metadata-file"_s),
                                                                 true);

            p.reset(PackagingJob::create(clp.positionalArguments().at(1),
                                         clp.positionalArguments().at(2),
                                         extraMetaDataMap,
                                         extraSignedMetaDataMap,
                                         clp.isSet(u"json"_s)));
            break;
        }
        case DevSignPackage:
            clp.addOption({ u"verbose"_s, u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,    u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"package"_s,        u"File name of the unsigned package (input)."_s);
            clp.addPositionalArgument(u"signed-package"_s, u"File name of the signed package (output)."_s);
            clp.addPositionalArgument(u"certificate"_s,    u"PKCS#12 certificate file."_s);
            clp.addPositionalArgument(u"password"_s,       u"Password for the PKCS#12 certificate."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 5)
                clp.showHelp(1);

            p.reset(PackagingJob::developerSign(clp.positionalArguments().at(1),
                                                clp.positionalArguments().at(2),
                                                clp.positionalArguments().at(3),
                                                clp.positionalArguments().at(4),
                                                clp.isSet(u"json"_s)));
            break;

        case DevVerifyPackage:
            clp.addOption({ u"verbose"_s, u"Print details regarding the verification to stdout."_s });
            clp.addPositionalArgument(u"package"_s,      u"File name of the signed package (input)."_s);
            clp.addPositionalArgument(u"certificates"_s, u"The developer's CA certificate file(s)."_s, u"certificates..."_s);
            clp.process(a);

            if (clp.positionalArguments().size() < 3)
                clp.showHelp(1);

            p.reset(PackagingJob::developerVerify(clp.positionalArguments().at(1),
                                                  clp.positionalArguments().mid(2)));
            break;

        case StoreSignPackage:
            clp.addOption({ u"verbose"_s, u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,    u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"package"_s,        u"File name of the unsigned package (input)."_s);
            clp.addPositionalArgument(u"signed-package"_s, u"File name of the signed package (output)."_s);
            clp.addPositionalArgument(u"certificate"_s,    u"PKCS#12 certificate file."_s);
            clp.addPositionalArgument(u"password"_s,       u"Password for the PKCS#12 certificate."_s);
            clp.addPositionalArgument(u"hardware-id"_s,    u"Unique hardware id to which this package gets bound."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 6)
                clp.showHelp(1);

            p.reset(PackagingJob::storeSign(clp.positionalArguments().at(1),
                                            clp.positionalArguments().at(2),
                                            clp.positionalArguments().at(3),
                                            clp.positionalArguments().at(4),
                                            clp.positionalArguments().at(5),
                                            clp.isSet(u"json"_s)));
            break;

        case StoreVerifyPackage:
            clp.addOption({ u"verbose"_s, u"Print details regarding the verification to stdout."_s });
            clp.addPositionalArgument(u"package"_s,      u"File name of the signed package (input)."_s);
            clp.addPositionalArgument(u"certificates"_s, u"Store CA certificate file(s)."_s, u"certificates..."_s);
            clp.addPositionalArgument(u"hardware-id"_s,  u"Unique hardware id to which this package was bound."_s);
            clp.process(a);

            if (clp.positionalArguments().size() < 4)
                clp.showHelp(1);

            p.reset(PackagingJob::storeVerify(clp.positionalArguments().at(1),
                                              clp.positionalArguments().mid(2, clp.positionalArguments().size() - 3),
                                              *--clp.positionalArguments().cend()));
            break;

        case YamlToJson: {
            clp.addOption({{ u"i"_s, u"document-index"_s }, u"Only output the specified YAML sub-document."_s, u"index"_s });
            clp.addPositionalArgument(u"yaml-file"_s, u"YAML file name, defaults to stdin (input)."_s);
            clp.process(a);

            if (clp.positionalArguments().size() > 2)
                clp.showHelp(1);

            QString yamlName = (clp.positionalArguments().size() == 2) ? clp.positionalArguments().at(1)
                                                                       : u"-"_s;
            QFile yaml(yamlName);
            if (yamlName == u"-") {
                if (!yaml.open(0, QIODevice::ReadOnly))
                    throw Exception("Could not open stdin for reading");
            } else if (!yaml.open(QIODevice::ReadOnly)) {
                throw Exception(yaml, "Could not open YAML input file");
            }

            QVector<QVariant> docs = YamlParser::parseAllDocuments(yaml.readAll());
            QJsonDocument json;
            if (clp.isSet(u"i"_s)) {
                bool isInt;
                int index = clp.value(u"i"_s).toInt(&isInt);
                if (!isInt || index < 0) {
                    throw Exception("Invalid document index specified: %1").arg(clp.value(u"i"_s));
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
        if (clp.isSet(u"verbose"_s) && !p->output().isEmpty())
            fprintf(stdout, "%s\n", qPrintable(p->output()));
        return p->resultCode();
    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
}
