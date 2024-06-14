// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
#include <QGuiApplication>
#include <QIcon>
#if !defined(QT_NO_OPENGL)
#  include <private/qopenglcontext_p.h>
#endif
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

#include "global.h"
#include "logging.h"
#include "sharedmain.h"

#include "utilities.h"
#include "dbus-utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "startuptimer.h"
#include "unixsignalhandler.h"
#include "watchdog.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

static const QMap<int, QString> &openGLProfileNames()
{
    static const QMap<int, QString> map = {
        { QSurfaceFormat::NoProfile,            u"default"_s },
        { QSurfaceFormat::CoreProfile,          u"core"_s },
        { QSurfaceFormat::CompatibilityProfile, u"compatibility"_s }
    };
    return map;
}

bool QtAM::SharedMain::s_initialized = false;

SharedMain::SharedMain()
{
    if (Q_UNLIKELY(!s_initialized || !QCoreApplication::instance())) {
        qCCritical(LogSystem) << "ERROR: Q(Gui)Application must be instantiated after SharedMain::initialize "
                                 "has been called and before SharedMain is instantiated";
    }
#if !defined(QT_NO_OPENGL)
    if (!(QGuiApplicationPrivate::instance()
            && QGuiApplicationPrivate::instance()->platformIntegration()
            && QGuiApplicationPrivate::instance()->platformIntegration()->hasCapability(QPlatformIntegration::OpenGL))) {
        qCCritical(LogGraphics) << "No OpenGL capable Wayland client buffer integration available: "
                                   "this application can only use software rendering";
    }
#else
    qCCritical(LogGraphics) << "Qt has been compiled without OpenGL support: "
                               "this application will use software rendering";
#endif
}

SharedMain::~SharedMain()
{ }

// Initialization routine that needs to be called BEFORE the Q*Application constructor
void SharedMain::initialize()
{
    if (Q_UNLIKELY(QCoreApplication::instance()))
        qCCritical(LogSystem) << "ERROR: SharedMain::initialize must be called before Q(Gui)Application is instantiated";

    s_initialized = true;

#if !defined(QT_NO_OPENGL)
    // this is needed for both WebEngine and Wayland Multi-screen rendering
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Calling this semi-private function before the QGuiApplication constructor is the only way to
    // set a custom global shared GL context. We are NOT creating it right now, since we still need
    // to parse the requested format from the config files - the creation is completed later
    // in setupOpenGL().
    qt_gl_set_global_share_context(new QOpenGLContext());
#endif

#if defined(Q_OS_UNIX) && defined(AM_MULTI_PROCESS)
    // set a reasonable default for OSes/distros that do not set this by default
    if (!qEnvironmentVariableIsSet("XDG_RUNTIME_DIR"))
        setenv("XDG_RUNTIME_DIR", QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation).toLocal8Bit(), 1);
#endif

    ensureLibDBusIsAvailable();
}

// We need to do some things BEFORE the Q*Application constructor runs, so we're using this
// old trick to do this hooking transparently for the user of the class.
int &SharedMain::preConstructor(int &argc)
{
    initialize();
    return argc;
}

void SharedMain::setupIconTheme(const QStringList &themeSearchPaths, const QString &themeName)
{
    QIcon::setThemeSearchPaths(themeSearchPaths);
    QIcon::setThemeName(themeName);
}

void SharedMain::setupQmlDebugging(bool qmlDebugging)
{
    bool hasJSDebugArg = !static_cast<QCoreApplicationPrivate *>
                         (QObjectPrivate::get(qApp))->qmljsDebugArgumentsString().isEmpty();

    if (hasJSDebugArg || qmlDebugging) {
#if !defined(QT_NO_QML_DEBUGGER)
        QQmlDebuggingEnabler::enableDebugging(true);
        if (!QLoggingCategory::defaultCategory()->isDebugEnabled()) {
            qCCritical(LogRuntime) << "The default 'debug' logging category was disabled. "
                                      "Re-enabling it for the QML Debugger interface to work correctly.";
            QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
        }
#else
        qCWarning(LogSystem) << "The --qml-debug/-qmljsdebugger options are ignored, "
                                "because Qt was built without support for QML Debugging!";
#endif
    }
}

void SharedMain::setupWatchdog(std::chrono::milliseconds eventloopCheckInterval,
                               std::chrono::milliseconds eventloopWarn,
                               std::chrono::milliseconds eventloopKill,
                               std::chrono::milliseconds quickWindowCheckInterval,
                               std::chrono::milliseconds quickWindowSyncWarn,
                               std::chrono::milliseconds quickWindowSyncKill,
                               std::chrono::milliseconds quickWindowRenderWarn,
                               std::chrono::milliseconds quickWindowRenderKill,
                               std::chrono::milliseconds quickWindowSwapWarn,
                               std::chrono::milliseconds quickWindowSwapKill)
{
    auto *wd = Watchdog::create();

    wd->setThreadTimeouts(eventloopCheckInterval, eventloopWarn, eventloopKill);
    wd->setQuickWindowTimeouts(quickWindowCheckInterval, quickWindowSyncWarn,
                               quickWindowSyncKill, quickWindowRenderWarn,
                               quickWindowRenderKill, quickWindowSwapWarn,
                               quickWindowSwapKill);
}

void SharedMain::setupLogging(bool verbose, const QStringList &loggingRules,
                              const QString &messagePattern, const QVariant &useAMConsoleLogger)
{
    static const QStringList verboseRules {
        u"*=true"_s,
        u"qt.*.debug=false"_s,
        u"am.wayland.debug=false"_s,
        u"qt.qml.overloadresolution.info=false"_s,
        u"qt.widgets.painting.info=false"_s,
    };

    const QStringList rules = verbose ? verboseRules
                                      : loggingRules.isEmpty() ? QStringList(u"*.debug=false"_s)
                                                               : loggingRules;
    Logging::setFilterRules(rules);
    Logging::setMessagePattern(messagePattern);
    Logging::setUseAMConsoleLogger(useAMConsoleLogger);
    Logging::completeSetup();
    StartupTimer::instance()->checkpoint("after logging setup");
}

void SharedMain::setupOpenGL(const OpenGLConfiguration &openGLConfiguration)
{
#if !defined(QT_NO_OPENGL)
    QString profileName = openGLConfiguration.desktopProfile;
    int majorVersion = openGLConfiguration.esMajorVersion;
    int minorVersion = openGLConfiguration.esMinorVersion;

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
        int profile = openGLProfileNames().key(profileName, -1);

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

    qAddPostRoutine([]() { delete qt_gl_global_share_context(); });

    StartupTimer::instance()->checkpoint("after OpenGL setup");

#else
    Q_UNUSED(openGLConfiguration)
#endif
}

void SharedMain::checkOpenGLFormat(const char *what, const QSurfaceFormat &format) const
{
#if !defined(QT_NO_OPENGL)
    if ((m_requestedOpenGLProfile != QSurfaceFormat::NoProfile)
            && (format.profile() != m_requestedOpenGLProfile)) {
        qCWarning(LogGraphics) << "Failed to get the requested OpenGL profile"
                               << openGLProfileNames().value(m_requestedOpenGLProfile) << "for the"
                               << what << "- got"
                               << openGLProfileNames().value(format.profile()) << "instead.";
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
#else
    Q_UNUSED(what)
    Q_UNUSED(format)
#endif
}

QT_END_NAMESPACE_AM
