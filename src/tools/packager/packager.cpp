// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
// Qt-Security score:critical reason:data-parser

#include <iostream>
#include <memory>

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
#include <QtAppManPackage/packageutilities.h>
#include "packagingjob.h"
#include "../shared/toolapplication.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

// REMEMBER to update the completion file util/bash/appman-prompt, if you add new commands or options!

enum Command {
    NoCommand = 0,
    CreatePackage,
    DevSignPackage,
    DevVerifyPackage,
    StoreSignPackage,
    StoreVerifyPackage,
    YamlToJson
};

int main(int argc, char *argv[])
{
    ToolApplication<Command> tool("Packager", argc, argv);

    tool.setCommands({
        { CreatePackage,      "create-package",       "Create a new package." },
        { DevSignPackage,     "dev-sign-package",     "Add developer signature to package." },
        { DevVerifyPackage,   "dev-verify-package",   "Verify developer signature on package." },
        { StoreSignPackage,   "store-sign-package",   "Add store signature to package." },
        { StoreVerifyPackage, "store-verify-package", "Verify store signature on package." },
        { YamlToJson,         "yaml-to-json",         "Convenience functionality for build systems (internal)." }
    });

    QCommandLineParser clp;
    Command cmd = tool.parse(clp);

    try {
        std::unique_ptr<PackagingJob> p;

        switch (cmd) {
        default:
        case NoCommand:
            break;

        case CreatePackage: {
            clp.addOption({ u"verbose"_s, u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,    u"Output in JSON format instead of YAML."_s });
            clp.addOption({{ u"extra-metadata"_s,      u"m"_s }, u"Add extra meta-data to the package, supplied on the command line."_s, u"yaml-snippet"_s });
            clp.addOption({{ u"extra-metadata-file"_s, u"M"_s }, u"Add extra meta-data to the package, read from file."_s, u"yaml-file"_s });
            clp.addOption({{ u"extra-signed-metadata"_s,      u"s"_s }, u"Add extra, digitally signed, meta-data to the package, supplied on the command line."_s, u"yaml-snippet"_s });
            clp.addOption({{ u"extra-signed-metadata-file"_s, u"S"_s }, u"Add extra, digitally signed, meta-data to the package, read from file."_s, u"yaml-file"_s });
            clp.addOption({{ u"include-extended-attributes"_s, u"x"_s }, u"Include xattrs on files and directories, if available."_s });
            clp.addOption({{ u"pre-package-command"_s, u"p"_s }, u"Calls this command on each packaged file, before it gets packaged (split on whitespace, no shell-style quoting)."_s , u"command"_s });
            clp.addPositionalArgument(u"package"_s,          u"The file name of the created package."_s);
            clp.addPositionalArgument(u"source-directory"_s, u"The package's content root directory."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 3)
                clp.showHelp(1);

            auto parseYamlMetada = [](const QStringList &metadataSnippets, const QStringList &metadataFiles, bool isSigned) -> QVariantMap {
                QVariantMap result;
                QVector<std::pair<QByteArray, QString>> metadata;

                for (const QString &file : metadataFiles) {
                    QFile f(file);
                    if (!f.open(QIODevice::ReadOnly))
                        throw Exception(f, "Could not open metadata file for reading");
                    metadata.append(std::make_pair(f.readAll(), file));
                }
                for (const QString &snippet : metadataSnippets)
                    metadata.append(std::make_pair(snippet.toUtf8(), QString()));

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
                    for (const auto &doc : std::as_const(docs))
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
                                         clp.isSet(u"include-extended-attributes"_s),
                                         clp.value(u"pre-package-command"_s),
                                         clp.isSet(u"json"_s)));
            break;
        }
        case DevSignPackage: {
            clp.addOption({ u"verbose"_s, u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,    u"Output in JSON format instead of YAML."_s });
            clp.addOption({{ u"p"_s, u"password"_s },
                           u"Password for the PKCS#12 certificate in the form "
                           "pass:<password>, env:<envvar>, file:<path>, fd:<number> or stdin. "
                           "See the documentation for details."_s,
                           u"format[:value]"_s });
            clp.addPositionalArgument(u"package"_s,        u"File name of the unsigned package (input)."_s);
            clp.addPositionalArgument(u"signed-package"_s, u"File name of the signed package (output)."_s);
            clp.addPositionalArgument(u"certificate"_s,    u"PKCS#12 certificate file."_s);
            clp.process(tool);

            const qsizetype argCount = clp.positionalArguments().size();
            QString password;

            if ((argCount < 4) || (argCount > 5))
                clp.showHelp(1);
            if (argCount == 5) {
                if (clp.isSet(u"p"_s)) {
                    throw Exception("Cannot use --password and the legacy password positional "
                                    "argument at the same time.");
                }
                std::cerr << "INFO: Using the legacy password positional argument is deprecated. "
                             "Please use the --password option instead." << std::endl;
                password = clp.positionalArguments().at(4);
            } else {
                password = tool.parsePasswordOption(clp.value(u"p"_s),
                                                    u"PKCS#12 certificate password"_s);
            }

            p.reset(PackagingJob::developerSign(clp.positionalArguments().at(1),
                                                clp.positionalArguments().at(2),
                                                clp.positionalArguments().at(3),
                                                password,
                                                clp.isSet(u"json"_s)));
            break;
        }
        case DevVerifyPackage:
            clp.addOption({ u"verbose"_s, u"Print details regarding the verification to stdout."_s });
            clp.addOption({ u"crl"_s, u"CRL file to use during verification."_s, u"file"_s });
            clp.addPositionalArgument(u"package"_s,      u"File name of the signed package (input)."_s);
            clp.addPositionalArgument(u"certificates"_s, u"Developer CA certificate file(s)."_s, u"certificates..."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() < 3)
                clp.showHelp(1);

            p.reset(PackagingJob::developerVerify(clp.positionalArguments().at(1),
                                                  clp.positionalArguments().mid(2),
                                                  clp.values(u"crl"_s)));
            break;

        case StoreSignPackage: {
            clp.addOption({ u"verbose"_s,     u"Dump the package's meta-data header and footer information to stdout."_s });
            clp.addOption({ u"json"_s,        u"Output in JSON format instead of YAML."_s });
            clp.addOption({ u"hardware-id"_s, u"Unique hardware id to which this package gets bound."_s, u"hardware-id"_s });
            clp.addOption({{ u"p"_s, u"password"_s },
                           u"Password for the PKCS#12 certificate in the form "
                           "pass:<password>, env:<envvar>, file:<path>, fd:<number> or stdin. "
                           "See the documentation for details."_s,
                           u"format[:value]"_s });
            clp.addPositionalArgument(u"package"_s,        u"File name of the unsigned package (input)."_s);
            clp.addPositionalArgument(u"signed-package"_s, u"File name of the signed package (output)."_s);
            clp.addPositionalArgument(u"certificate"_s,    u"PKCS#12 certificate file."_s);
            clp.process(tool);

            const qsizetype argCount = clp.positionalArguments().size();
            QString password;
            QString hardwareId;

            if ((argCount < 4) || (argCount > 6))
                clp.showHelp(1);
            if (argCount > 4) {
                if (clp.isSet(u"p"_s)) {
                    throw Exception("Cannot use --password and the legacy password positional "
                                    "argument at the same time.");
                }
                std::cerr << "INFO: Using the legacy password positional argument is deprecated. "
                             "Please use the --password option instead." << std::endl;
                password = clp.positionalArguments().at(4);

                if (argCount > 5) {
                    if (clp.isSet(u"hardware-id"_s)) {
                        throw Exception("Cannot use --hardware-id and the legacy hardware-id positional "
                                        "argument at the same time.");
                    }
                    std::cerr << "INFO: Using the legacy hardware-id positional argument is deprecated. "
                                 "Please use the --hardware-id option instead." << std::endl;
                    hardwareId = clp.positionalArguments().at(5);
                }
            } else {
                password = tool.parsePasswordOption(clp.value(u"p"_s),
                                                    u"PKCS#12 certificate password"_s);
                hardwareId = clp.value(u"hardware-id"_s);
            }

            p.reset(PackagingJob::storeSign(clp.positionalArguments().at(1),
                                            clp.positionalArguments().at(2),
                                            clp.positionalArguments().at(3),
                                            password,
                                            hardwareId,
                                            clp.isSet(u"json"_s)));
            break;
        }
        case StoreVerifyPackage:
            clp.addOption({ u"verbose"_s, u"Print details regarding the verification to stdout."_s });
            clp.addOption({ u"crl"_s, u"CRL file to use during verification."_s, u"file"_s });
            clp.addPositionalArgument(u"package"_s,      u"File name of the signed package (input)."_s);
            clp.addPositionalArgument(u"certificates"_s, u"Store CA certificate file(s)."_s, u"certificates..."_s);
            clp.addPositionalArgument(u"hardware-id"_s,  u"Unique hardware id to which this package was bound."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() < 4)
                clp.showHelp(1);

            p.reset(PackagingJob::storeVerify(clp.positionalArguments().at(1),
                                              clp.positionalArguments().mid(2, clp.positionalArguments().size() - 3),
                                              clp.values(u"crl"_s),
                                              *--clp.positionalArguments().cend()));
            break;

        case YamlToJson: {
            clp.addOption({{ u"i"_s, u"document-index"_s }, u"Only output the specified YAML sub-document."_s, u"index"_s });
            clp.addPositionalArgument(u"yaml-file"_s, u"YAML file name, defaults to stdin (input)."_s);
            clp.process(tool);

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
            std::cout << json.toJson(QJsonDocument::Indented).constData();
            return 0;
        }
        }

        if (!p)
            return 2;

        p->execute();
        if (clp.isSet(u"verbose"_s) && !p->output().isEmpty())
            std::cout << qPrintable(p->output()) << '\n';
        return p->resultCode();
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 2;
    }
}
