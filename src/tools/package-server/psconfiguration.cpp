// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/exception.h>
#include "psconfiguration.h"


QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;
using namespace QtYaml;

const QString PSConfiguration::DefaultListenAddress  = u"localhost:8020"_s;
const QString PSConfiguration::DefaultConfigFile     = u"amps-config.yaml"_s;
const QString PSConfiguration::DefaultProjectId      = u"PROJECT"_s;

static const QString ConfigFormat = u"amps-configuration"_s;
static const int ConfigVersion = 1;


void PSConfiguration::parse(const QStringList &args)
{
    QCommandLineParser clp;
    clp.setApplicationDescription(u"\n"_s + QCoreApplication::applicationName());

    clp.addOptions({
        { { u"h"_s, u"help"_s },
         u"Displays help on commandline options."_s
        },
        { { u"version"_s },
         u"Displays version information."_s
        },
        { { u"dd"_s, u"data-directory"_s },
         u"The data directory to use. (default: the current directory, but only if it contains an "_s + DefaultConfigFile + u" file)"_s, u"dir"_s
        },
        { { u"la"_s, u"listen-address"_s },
         u"The network address to listen on: <ip>|any|localhost[:port]. (default: "_s + DefaultListenAddress + u')', u"address"_s
        },
        { { u"pi"_s, u"project-id"_s },
         u"Set the project id to a non-empty ASCII string. (default: "_s + DefaultProjectId + u')', u"id"_s
        },
        { { u"sc"_s /*, u"store-sign-certificate"_s*/ },
         u"The certificate file for signing store downloads."_s, u"file"_s
        },
        { { u"sp"_s /*, u"store-sign-password"_s*/ },
         u"The password for the store signing certificate"_s, u"password"_s
        },
        { { u"dc"_s /*, u"developer-verification-ca-certificate"_s*/ },
         u"The CA certificate files to verify developer signatures on upload."_s, u"files..."_s
        },
        { { u"cc"_s, u"create-config"_s },
            u"Save the current configuration to an "_s + DefaultConfigFile + u" file in the data directory."_s
        },
    });

    if (!clp.parse(args)) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }

    if (clp.isSet(u"version"_s))
        clp.showVersion();
    if (clp.isSet(u"help"_s))
        clp.showHelp();

    projectId = DefaultProjectId;
    listenAddress = DefaultListenAddress;

    QString dd = clp.value(u"dd"_s);
    if (dd.isEmpty()) {
        if (!QDir::current().exists(DefaultConfigFile))
            clp.showHelp();
        dataDirectory = QDir::current();
    } else {
        dataDirectory.setPath(dd);
    }

    if (dataDirectory.exists() && dataDirectory.exists(DefaultConfigFile)) {
        // parse the config file
        try {
            QFile f(dataDirectory.filePath(DefaultConfigFile));
            if (!f.open(QIODevice::ReadOnly))
                throw Exception(f, "failed to open config file");

            YamlParser yp(f.readAll(), f.fileName());
            auto header = yp.parseHeader();
            if ((header.first != ConfigFormat) || (header.second != ConfigVersion)) {
                throw Exception("Unsupported format type and/or version (expected: %1 / %2)")
                    .arg(ConfigFormat).arg(ConfigVersion);
            }
            yp.nextDocument();

            YamlParser::Fields fields = {
                { "listenAddress", false, YamlParser::Scalar, [this](YamlParser *p) {
                     listenAddress = p->parseString();
                     if (listenAddress.isEmpty())
                         throw YamlParserException(p, "the listenAddress field cannot be empty");
                 } },
                { "projectId", false, YamlParser::Scalar, [this](YamlParser *p) {
                     projectId = p->parseString();
                     if (projectId.isEmpty())
                         throw YamlParserException(p, "the projectId field cannot be empty");
                 } },
                { "storeSignCertificate", false, YamlParser::Scalar, [this](YamlParser *p) {
                     storeSignCertificateFile = p->parseString();
                 } },
                { "storeSignPassword", false, YamlParser::Scalar, [this](YamlParser *p) {
                     storeSignPassword = p->parseString();
                 } },
                { "developerVerificationCaCertificates", false, YamlParser::Scalar | YamlParser::List, [this](YamlParser *p) {
                     developerVerificationCaCertificateFiles = p->parseStringOrStringList();
                 } },
            };

            yp.parseFields(fields);
        } catch (const Exception &e) {
            throw Exception("Failed to parse config file %1: %2").arg(DefaultConfigFile, e.errorString());
        }
    }

    // now override any settings from the config file with command line options
    if (clp.isSet(u"la"_s))
        listenAddress = clp.value(u"la"_s);
    if (clp.isSet(u"pi"_s)) {
        projectId = clp.value(u"pi"_s);
        if (projectId.isEmpty())
            throw Exception("the option --pi/--project-id cannot be an empty string");
    }
    if (clp.isSet(u"sc"_s))
        storeSignCertificateFile = clp.value(u"sc"_s);
    if (clp.isSet(u"sp"_s))
        storeSignPassword = clp.value(u"sp"_s);
    if (clp.isSet(u"dc"_s))
        developerVerificationCaCertificateFiles = clp.values(u"dc"_s);

    // post-processing
    listenUrl = QUrl::fromUserInput(listenAddress);
    if (listenUrl.host().isEmpty())
        throw Exception("the listen address needs to have a host part: either an IP address, 'any' or 'localhost'");

    // load all the certificates
    if (!storeSignCertificateFile.isEmpty()) {
        QFile sscf(storeSignCertificateFile);
        if (!sscf.open(QIODevice::ReadOnly))
            throw Exception(sscf, "could not open store-sign certificate file");
        storeSignCertificate = sscf.readAll();
    }

    for (const QString &cert : std::as_const(developerVerificationCaCertificateFiles)) {
        QFile dccf(cert);
        if (!dccf.open(QIODevice::ReadOnly))
            throw Exception(dccf, "could not open developer-ca certificate file");
        developerVerificationCaCertificates << dccf.readAll();
    }

    // data directory
    if (!dataDirectory.mkpath(u"."_s))
        throw Exception("could not create the data directory %1").arg(dataDirectory.absolutePath());
    if (!dataDirectory.exists())
        throw Exception("not a directory: %1").arg(dataDirectory.absolutePath());

    // save the config as YAML
    if (clp.isSet(u"cc"_s)) {
        static const QVariantMap header {
            { u"formatType"_s, ConfigFormat },
            { u"formatVersion"_s, ConfigVersion }
        };

        QVariantMap config;
        if (listenAddress != DefaultListenAddress)
            config.insert(u"listenAddress"_s, listenAddress);
        if (projectId != DefaultProjectId)
            config.insert(u"projectId"_s, projectId);
        if (!storeSignCertificateFile.isEmpty())
            config.insert(u"storeSignCertificate"_s, storeSignCertificateFile);
        if (!storeSignPassword.isEmpty())
            config.insert(u"storeSignPassword"_s, storeSignPassword);
        if (!developerVerificationCaCertificateFiles.isEmpty())
            config.insert(u"developerVerificationCaCertificates"_s, developerVerificationCaCertificateFiles);

        auto yaml = QtYaml::yamlFromVariantDocuments({ header, config });
        QFile f(dataDirectory.absoluteFilePath(DefaultConfigFile));
        if (!f.open(QIODevice::WriteOnly) || (f.write(yaml) != yaml.size()))
            throw Exception(f, "failed to save configuration as YAML");
    }
}
