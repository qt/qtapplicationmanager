/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "global.h"
#include "logging.h"
#include "main.h"
#include "defaultconfiguration.h"
#include "package.h"
#if !defined(AM_DISABLE_INSTALLER)
#  include "sudo.h"
#endif
#include "startuptimer.h"

#if defined(AM_TESTRUNNER)
#  include "testrunner.h"
#endif


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    StartupTimer::instance()->checkpoint("entered main");

#if defined(AM_TESTRUNNER)
    QCoreApplication::setApplicationName(qSL("ApplicationManager QML Test Runner"));
#else
    QCoreApplication::setApplicationName(qSL("ApplicationManager"));
#endif
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-dlt-logging", argv[i]) == 0) {
            Logging::setDltEnabled(false);
            break;
        }
    }
    Logging::initialize();
    StartupTimer::instance()->checkpoint("after basic initialization");

#if !defined(AM_DISABLE_INSTALLER)
    Package::ensureCorrectLocale();

    QString error;
    if (Q_UNLIKELY(!forkSudoServer(DropPrivilegesPermanently, &error))) {
        qCCritical(LogSystem) << "ERROR:" << qPrintable(error);
        return 2;
    }
    StartupTimer::instance()->checkpoint("after sudo server fork");
#endif

    try {
        Main a(argc, argv);

#if defined(AM_TESTRUNNER)
        const char *additionalDescription =
                "Additional testrunner commandline options can be set after the -- argument\n" \
                "Use -- -help to show all available testrunner commandline options.";
        bool onlyOnePositionalArgument = false;
#else
        const char *additionalDescription = nullptr;
        bool onlyOnePositionalArgument = true;
#endif

        DefaultConfiguration cfg(additionalDescription, onlyOnePositionalArgument);
        cfg.parse();

        StartupTimer::instance()->checkpoint("after command line parse");
#if defined(AM_TESTRUNNER)
        TestRunner::initialize(cfg.testRunnerArguments());
#endif
        a.setup(&cfg);
#if defined(AM_TESTRUNNER)
        a.qmlEngine()->rootContext()->setContextProperty("buildConfig", cfg.buildConfig());
#endif
        a.loadQml(cfg.loadDummyData());
        a.showWindow(cfg.fullscreen() && !cfg.noFullscreen());

#if defined(AM_TESTRUNNER)
        return TestRunner::exec(a.qmlEngine());
#else
        return MainBase::exec();
#endif
    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        return 2;
    }
}
