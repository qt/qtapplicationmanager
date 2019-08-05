/****************************************************************************
**
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

#include <QtAppManCommon/global.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QString>
#include <QMap>

const QString descriptionParameter = qSL("description");
const QString summaryParameter = qSL("summary");
const QString shortDescriptionParameter = qSL("short-description");
const QString categoryParameter = qSL("category");
const QString vendorParameter = qSL("vendor");
const QString usernameParameter = qSL("username");
const QString passwordParameter = qSL("password");

const QString applicationName = qSL("Deployment Server remote upload tool");

const QString serverReplyOk = qSL("ok");
const QString serverReplyStatus = qSL("status");
const QString invalidServerReply = qSL("Invalid server reply");


QString getCurrentUsername()
{
    QString username = QString::fromLatin1(qgetenv("USER")); // For Linux/QNX/macOS
    if (username.isEmpty())
        username = QString::fromLatin1(qgetenv("USERNAME")); // For windows
    return username;
}

QString makeBasicAuth(const QString& username,const QString& password)
{
    QString header = qSL("Basic ");
    QString base64data = username + qSL(":") + password;
    //Encode string in base64, and convert it back to string.
    //toUtf8 is used, because password can be non-ascii
    //fromLatin1 is used, because base64 is latin1 by definition.
    base64data = QString::fromLatin1(base64data.toUtf8().toBase64());
    header = header + base64data;
    return header;
}

Q_NORETURN void printErrorAndExit(const QString &errorMessage, int exitCode)
{
    fprintf(stderr, "%s\n", qPrintable(errorMessage));
    exit(exitCode);
}

Q_NORETURN void printErrorAndExit(const QString &errorPrefix, const QString &errorMessage, int exitCode)
{
    fprintf(stderr, "%s: %s\n", qPrintable(errorPrefix), qPrintable(errorMessage));
    exit(exitCode);
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(applicationName);
    QCoreApplication::setOrganizationName(qSL("QtProject"));
    QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));
    QCoreApplication a(argc, argv);

    QCommandLineParser clp;
    clp.setApplicationDescription(qSL("\n") + QCoreApplication::applicationName());
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    clp.addHelpOption();
    clp.addVersionOption();
    clp.addOption({{usernameParameter, qSL("u")}, qSL("User for uploading package. Defaults to current user"), usernameParameter, getCurrentUsername()});
    clp.addOption({{passwordParameter, qSL("p")}, qSL("Password for uploading package"), passwordParameter});
    clp.addOption({{categoryParameter, qSL("c")}, qSL("Category name or id for the package"), categoryParameter});
    clp.addOption({vendorParameter, qSL("Vendor name or id for the package"), vendorParameter});
    clp.addOption({summaryParameter, qSL("Short description"), qSL("Description string")});
    clp.addOption({descriptionParameter, qSL("Long description"), qSL("Description string")});
    clp.addOption({{qSL("url"), qSL("s")}, qSL("Server base url (with port)"), qSL("url"), qSL("http://localhost:8080")});
    clp.addPositionalArgument(qSL("package"), qSL("The file name of uploaded package"));
    clp.process(a);
    if (!clp.parse(QCoreApplication::arguments()))
        printErrorAndExit(clp.errorText(), 1);

    if (clp.positionalArguments().isEmpty())
        clp.showHelp(2);

    QString username;
    QString password;
    username = clp.value(usernameParameter);
    if (clp.isSet(passwordParameter))
        password = clp.value(passwordParameter);

    // Fill in the parameter map
    QMap<QString, QString> parametersList;
    if (clp.isSet(descriptionParameter))
        parametersList.insert(descriptionParameter, clp.value(descriptionParameter));
    if (clp.isSet(summaryParameter))
        parametersList.insert(shortDescriptionParameter, clp.value(summaryParameter));
    if (clp.isSet(categoryParameter))
        parametersList.insert(categoryParameter, clp.value(categoryParameter));
    if (clp.isSet(vendorParameter))
        parametersList.insert(vendorParameter, clp.value(vendorParameter));

    QNetworkAccessManager am;
    QNetworkRequest request(QUrl(clp.value(qSL("url")) + qSL("/upload")));
    // Setting this header is done to avoid two accesses to the server, first one
    // unauthenticated, second one authenticated, so authentication is forced by
    // adding a header
    request.setRawHeader("Authorization", makeBasicAuth(username, password).toLatin1());
    QHttpMultiPart *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // Sending parameters to server
    for (auto parameter = parametersList.cbegin(); parameter != parametersList.cend(); ++parameter) {
        QHttpPart requestParameters;
        QString dispositionHeader = qSL("form-data; name=\"") + parameter.key() + qSL("\"");
        requestParameters.setHeader(QNetworkRequest::ContentDispositionHeader, dispositionHeader);
        requestParameters.setBody(parameter.value().toUtf8());
        multipart->append(requestParameters);
    }

    // Sending file to server
    QHttpPart packagePart;
    packagePart.setHeader(QNetworkRequest::ContentTypeHeader, qSL("application/octet-stream"));
    packagePart.setHeader(QNetworkRequest::ContentDispositionHeader, qSL("form-data; name=\"package\";filename=\"package.appkg\""));
    QFile *packageFile = new QFile(clp.positionalArguments().first());
    if (!packageFile->open(QIODevice::ReadOnly))
        printErrorAndExit(packageFile->errorString(), 3);
    packagePart.setBodyDevice(packageFile);
    multipart->append(packagePart);
    packageFile->setParent(multipart);

    QNetworkReply *reply = am.post(request, multipart);
    multipart->setParent(reply);
    QEventLoop eventLoop; // QNetworkAcessManager does not work without event loop. At all.
    QObject::connect(reply, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
    if (reply->error() != QNetworkReply::NoError)
        printErrorAndExit(qSL("Network error"), reply->errorString(), 4);
    QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    if (document.isNull()) // Invalid reply
        printErrorAndExit(invalidServerReply, 5);
    if (!document[serverReplyStatus].isString()) // Invalid reply
        printErrorAndExit(invalidServerReply, 5);
    if (document[serverReplyStatus].toString() != serverReplyOk)
        printErrorAndExit(qSL("Error uploading package"), document[serverReplyStatus].toString(), 6);
    return 0;
}
