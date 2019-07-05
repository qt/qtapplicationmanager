/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include <memory>
#include <qglobal.h>

#include <QFile>
#include <QDir>
#include <QStringList>
#include <QVariant>
#include <QFileInfo>
#include <QLibrary>
#include <QStandardPaths>
#include <QQmlDebuggingEnabler>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtCore/private/qcoreapplication_p.h>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QIcon>
#  include <private/qopenglcontext_p.h>
#endif

#include "global.h"
#include "logging.h"
#include "sharedmain.h"

#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "unixsignalhandler.h"

#include "../plugin-interfaces/startupinterface.h"


QT_BEGIN_NAMESPACE_AM

#if !defined(AM_HEADLESS)
static QMap<int, QString> openGLProfileNames = {
    { QSurfaceFormat::NoProfile,            qSL("default") },
    { QSurfaceFormat::CoreProfile,          qSL("core") },
    { QSurfaceFormat::CompatibilityProfile, qSL("compatibility") }
};
#endif

SharedMain::SharedMain()
{ }

SharedMain::~SharedMain()
{
    delete m_debuggingEnabler;
}

// We need to do some things BEFORE the Q*Application constructor runs, so we're using this
// old trick to do this hooking transparently for the user of the class.
int &SharedMain::preConstructor(int &argc)
{
#if !defined(AM_HEADLESS)
#  if !defined(QT_NO_SESSIONMANAGER)
        QGuiApplication::setFallbackSessionManagementEnabled(false);
#  endif

    // this is needed for both WebEngine and Wayland Multi-screen rendering
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Calling this semi-private function before the QGuiApplication constructor is the only way to
    // set a custom global shared GL context. We are NOT creating it right now, since we still need
    // to parse the requested format from the config files - the creation is completed later
    // in setupOpenGL().
    qt_gl_set_global_share_context(new QOpenGLContext());

#  if defined(Q_OS_UNIX) && defined(AM_MULTI_PROCESS)
    // set a reasonable default for OSes/distros that do not set this by default
    if (!qEnvironmentVariableIsSet("XDG_RUNTIME_DIR"))
        setenv("XDG_RUNTIME_DIR", QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation).toLocal8Bit(), 1);
#  endif
#endif
    return argc;
}

void SharedMain::setupIconTheme(const QStringList &themeSearchPaths, const QString &themeName)
{
#if defined(AM_HEADLESS)
    Q_UNUSED(themeSearchPaths)
    Q_UNUSED(themeName)
#else
    QIcon::setThemeSearchPaths(themeSearchPaths);
    QIcon::setThemeName(themeName);
#endif
}

void SharedMain::setupQmlDebugging(bool qmlDebugging)
{
    bool hasJSDebugArg = !static_cast<QCoreApplicationPrivate *>(QObjectPrivate::get(qApp))->qmljsDebugArgumentsString().isEmpty();

    if (hasJSDebugArg || qmlDebugging) {
#if !defined(QT_NO_QML_DEBUGGER)
        m_debuggingEnabler = new QQmlDebuggingEnabler(true);
        if (!QLoggingCategory::defaultCategory()->isDebugEnabled()) {
            qCCritical(LogQmlRuntime) << "The default 'debug' logging category was disabled. "
                                         "Re-enabling it for the QML Debugger interface to work correctly.";
            QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
        }
#else
        qCWarning(LogSystem) << "The --qml-debug/-qmljsdebugger options are ignored, because Qt was built without support for QML Debugging!";
#endif
    }
}

void SharedMain::setupLoggingRules(bool verbose, const QStringList &loggingRules)
{
    const QStringList rules = verbose ? QStringList() << qSL("*=true") << qSL("qt.*.debug=false")
                                      : loggingRules.isEmpty() ? QStringList(qSL("*.debug=false"))
                                                               : loggingRules;
    Logging::setFilterRules(rules);
    StartupTimer::instance()->checkpoint("after logging setup");
}

void SharedMain::setupOpenGL(const QVariantMap &openGLConfiguration)
{
#if defined(AM_HEADLESS)
    Q_UNUSED(openGLConfiguration)
#else
    QString profileName = openGLConfiguration.value(qSL("desktopProfile")).toString();
    int majorVersion = openGLConfiguration.value(qSL("esMajorVersion"), -1).toInt();
    int minorVersion = openGLConfiguration.value(qSL("esMinorVersion"), -1).toInt();

    QOpenGLContext *globalContext = qt_gl_global_share_context();
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    bool isES = (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES);

    // either both are set or none is set
    if ((majorVersion > -1) != (minorVersion > -1)) {
        qCWarning(LogGraphics) << "Requesting only the major or minor OpenGL version number is not "
                                  "supported - always specify both or none.";
    } else if (majorVersion > -1) {
        bool valid = isES;

        if (!isES) {
            // we need to map the ES version to the corresponding desktop versions:
            static int mapping[] = {
                2, 0,   2, 1,
                3, 0,   4, 3,
                3, 1,   4, 5,
                3, 2,   4, 6,
                -1
            };
            for (int i = 0; mapping[i] != -1; i += 4) {
                if ((majorVersion == mapping[i]) && (minorVersion == mapping[i + 1])) {
                    majorVersion = mapping[i + 2];
                    minorVersion = mapping[i + 3];
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                qCWarning(LogGraphics).nospace() << "Requested OpenGLES version " << majorVersion
                                                 << "." << minorVersion
                                                 << ", but there is no mapping available to a corresponding desktop GL version.";
            }
        }

        if (valid) {
            format.setMajorVersion(majorVersion);
            format.setMinorVersion(minorVersion);
            m_requestedOpenGLMajorVersion = majorVersion;
            m_requestedOpenGLMinorVersion = minorVersion;
            qCDebug(LogGraphics).nospace() << "Requested OpenGL" << (isES ? "ES" : "") << " version "
                                           << majorVersion << "." << minorVersion;
        }
    }
    if (!profileName.isEmpty()) {
        int profile = openGLProfileNames.key(profileName, -1);

        if (profile == -1) {
            qCWarning(LogGraphics) << "Requested an invalid OpenGL profile:" << profileName;
        } else if (profile != QSurfaceFormat::NoProfile) {
            m_requestedOpenGLProfile = static_cast<QSurfaceFormat::OpenGLContextProfile>(profile);
            format.setProfile(m_requestedOpenGLProfile);
            qCDebug(LogGraphics) << "Requested OpenGL profile" << profileName;
        }
    }
    if ((m_requestedOpenGLProfile != QSurfaceFormat::NoProfile) || (m_requestedOpenGLMajorVersion > -1))
        QSurfaceFormat::setDefaultFormat(format);

    // Setting the screen is normally done by the QOpenGLContext constructor, but our constructor
    // ran before the QGuiApplication constructor, so the screen was not initialized yet. So we have
    // to tell the context about the screen again now:
    globalContext->setScreen(QGuiApplication::primaryScreen());

    if (!globalContext->create())
        qCWarning(LogGraphics) << "Failed to create the global shared OpenGL context.";

    // check if we got what we requested on the OpenGL side
    checkOpenGLFormat("global shared context", globalContext->format());
#endif
}

#if !defined(AM_HEADLESS)
void SharedMain::checkOpenGLFormat(const char *what, const QSurfaceFormat &format) const
{
    if ((m_requestedOpenGLProfile != QSurfaceFormat::NoProfile)
            && (format.profile() != m_requestedOpenGLProfile)) {
        qCWarning(LogGraphics) << "Failed to get the requested OpenGL profile"
                               << openGLProfileNames.value(m_requestedOpenGLProfile) << "for the"
                               << what << "- got"
                               << openGLProfileNames.value(format.profile()) << "instead.";
    }
    if (m_requestedOpenGLMajorVersion > -1) {
        if ((format.majorVersion() != m_requestedOpenGLMajorVersion )
                || (format.minorVersion() != m_requestedOpenGLMinorVersion)) {
            qCWarning(LogGraphics).nospace() << "Failed to get the requested OpenGL version "
                                             << m_requestedOpenGLMajorVersion << "."
                                             << m_requestedOpenGLMinorVersion << " for "
                                             << what << " - got "
                                             << format.majorVersion() << "."
                                             << format.minorVersion() << " instead";
        }
    }
}
#endif

QT_END_NAMESPACE_AM
