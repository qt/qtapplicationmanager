// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <memory>

#include <QCoreApplication>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/console.h>
#include <QtAppManPackage/packageutilities.h>
#include <QtAppManCrypto/cryptography.h>

#include "psconfiguration.h"
#include "pspackages.h"
#include "architecture.h"
#include "pshttpinterface.h"
#include "package-server.h"


QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;

ColorPrint QtAM::colorOut() { return ColorPrint(stdout, Console::stdoutSupportsAnsiColor()); }
ColorPrint QtAM::colorErr() { return ColorPrint(stderr, Console::stderrSupportsAnsiColor()); }


int main(int argc, char **argv)
{
    // enable OpenSSL3 to load old certificates
    Cryptography::enableOpenSsl3LegacyProvider();

    QCoreApplication::setApplicationName(u"Qt ApplicationManager Package Server"_s);
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(u""_s QT_AM_VERSION_STR);

    QCoreApplication a(argc, argv);

    try {
        auto cfg = std::make_unique<PSConfiguration>();
        cfg->parse(a.arguments());

        colorOut() << ColorPrint::bgreen << QCoreApplication::applicationName() << ColorPrint::reset;
        colorOut() << "> Data directory: " << ColorPrint::bmagenta << cfg->dataDirectory.absolutePath() << ColorPrint::reset;
        colorOut() << "> Project: " << ColorPrint::bcyan << cfg->projectId << ColorPrint::reset;
        colorOut() << "> Verify developer signature on upload: " << !cfg->developerVerificationCaCertificates.isEmpty();
        colorOut() << "> Add store signature on download: " << !cfg->storeSignCertificate.isEmpty();

        PSPackages packages(cfg.get());
        packages.initialize();

        PSHttpInterface http(cfg.get());
        http.listen();

        colorOut() << "> HTTP server listening on: " << ColorPrint::bblue << http.listenAddress() << ColorPrint::reset;

        http.setupRouting(&packages);

        return a.exec();
    } catch (const Exception &e) {
        colorErr() << ColorPrint::red << "ERROR" << ColorPrint::reset << ": " << e.errorString();
        return 2;
    }
}
