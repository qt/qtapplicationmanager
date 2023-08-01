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
#if !defined(AM_DISABLE_INSTALLER)
#  include "sudo.h"
#endif
#include "startuptimer.h"
#include "exception.h"
#include "qtyaml.h"

#if defined(AM_TESTRUNNER)
#  include "testrunner.h"
#endif


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

    QCoreApplication::setApplicationName(qSL("Qt Application Manager QML Test Runner"));
#else
    QCoreApplication::setApplicationName(qSL("Qt Application Manager"));
#endif
    QCoreApplication::setOrganizationName(qSL("QtProject"));
    QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    Logging::initialize(argc, argv);
    StartupTimer::instance()->checkpoint("after basic initialization");

    try {
#if !defined(AM_DISABLE_INSTALLER)
        Sudo::forkServer(Sudo::DropPrivilegesPermanently);
        StartupTimer::instance()->checkpoint("after sudo server fork");
#endif

        Main a(argc, argv);

        Configuration cfg(additionalDescription, onlyOnePositionalArgument);
        cfg.parseWithArguments(QCoreApplication::arguments());

        StartupTimer::instance()->checkpoint("after command line parse");
#if defined(AM_TESTRUNNER)
        TestRunner::setup(&cfg);
#endif
        a.setup(&cfg);
        a.loadQml(cfg.loadDummyData());
        a.showWindow(cfg.fullscreen() && !cfg.noFullscreen());

#if defined(AM_TESTRUNNER)
        return TestRunner::exec(a.qmlEngine());
#else
        return MainBase::exec();
#endif
    } catch (const Exception &e) {
        qCCritical(LogSystem).noquote() << "ERROR:" << e.errorString();
        return 2;
    }
}
