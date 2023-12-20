// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>

#include "global.h"
#include "logging.h"
#include "main.h"
#include "configuration.h"
#include "sudo.h"
#include "startuptimer.h"
#include "exception.h"
#include "qtyaml.h"

#if defined(AM_TESTRUNNER)
#  include "testrunner.h"
#endif

using namespace Qt::StringLiterals;


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    StartupTimer::instance()->checkpoint("entered main");

    const char *additionalDescription = nullptr;
    bool onlyOnePositionalArgument = true;

#if defined(AM_TESTRUNNER)
    additionalDescription =
        "Additional testrunner command line options can be set after the -- argument\n" \
        "Use -- -help to show all available testrunner command line options.";
    onlyOnePositionalArgument = false;

    QCoreApplication::setApplicationName(u"Qt Application Manager QML Test Runner"_s);
#else
    QCoreApplication::setApplicationName(u"Qt Application Manager"_s);
#endif
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(QString::fromLatin1(QT_AM_VERSION_STR));

    try {
        Main a(argc, argv, Main::InitFlag::ForkSudoServer | Main::InitFlag::InitializeLogging);

        Configuration cfg(additionalDescription, onlyOnePositionalArgument);
        cfg.parseWithArguments(QCoreApplication::arguments());

#if defined(AM_TESTRUNNER)
        TestRunner::setup(&cfg);
#endif
        a.setup(&cfg);
        a.loadQml(cfg.loadDummyData());
        a.showWindow(cfg.fullscreen() && !cfg.noFullscreen());

#if defined(AM_TESTRUNNER)
        return TestRunner::exec(a.qmlEngine());
#else
        return Main::exec();
#endif
    } catch (const Exception &e) {
        qCCritical(LogSystem).noquote() << "ERROR:" << e.errorString();
        return 2;
    }
}
