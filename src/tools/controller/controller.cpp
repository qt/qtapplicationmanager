// Copyright (C) 2025 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
// Qt-Security score:critical reason:data-parser

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>
#include <QDir>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusError>
#include <QMetaObject>
#include <QStringBuilder>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QLockFile>

#include <functional>
#include <iostream>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/unixsignalhandler.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/dbus-utilities.h>

#include "dbus.h"
#include "../shared/toolapplication.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

Q_GLOBAL_STATIC(DBus, dbus)

static void installInterruptHandler(const std::function<void (int)> &handler)
{
#if defined(Q_OS_UNIX)
#  define AM_SIGNALS  { SIGTERM, SIGINT, SIGPIPE, SIGHUP }
#else
#  define AM_SIGNALS  { SIGTERM, SIGINT }
#endif
    // on Ctrl+C or SIGTERM -> stop the application
    UnixSignalHandler::instance()->resetToDefault(AM_SIGNALS);
    UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler,
                                           AM_SIGNALS, handler);
}

static std::pair<QString, QMultiHash<QString, int>> runningInstanceIds();
static QVariantMap resolveInstanceInfo(const QString &instanceId);

static void startOrDebugApplication(const QString &debugWrapper, const QString &appId,
                                    const QMap<QString, int> &stdRedirections, bool restart,
                                    const QString &documentUrl) noexcept(false);
static void stopApplication(const QString &appId, bool forceKill = false) noexcept(false);
static void stopAllApplications() noexcept(false);
static void listApplications() noexcept(false);
static void showApplication(const QString &appId, bool asJson = false) noexcept(false);
static void listPackages() noexcept(false);
static void showPackage(const QString &packageId, bool asJson = false) noexcept(false);
static void installPackage(const QString &packageUrl, bool acknowledge) noexcept(false);
static void removePackage(const QString &packageId, bool keepDocuments, bool force) noexcept(false);
static void listInstallationTasks() noexcept(false);
static void cancelInstallationTask(bool all, const QString &singleTaskId) noexcept(false);
static void showInstallationLocation(bool asJson = false) noexcept(false);
static void listInstances() noexcept(false);
static void injectIntentRequest(const QString &intentId, bool isBroadcast,
                                const QString &applicationId, const QString &requestingApplicationId,
                                const QString &jsonParameters) noexcept(false);
static void setDeveloperCertificate(const QString &pkcs12Path, const QString &pkcs12Password,
                                    bool clear = false) noexcept(false);
static void showDevelopmentMode(bool asJson = false) noexcept(false);

// REMEMBER to update the completion file util/bash/appman-prompt, if you add new commands or options!

enum Command {
    NoCommand = 0,
    StartApplication,
    DebugApplication,
    StopApplication,
    StopAllApplications,
    ListApplications,
    ShowApplication,
    ListPackages,
    ShowPackage,
    InstallPackage,
    RemovePackage,
    ListInstallationTasks,
    CancelInstallationTask,
    ShowInstallationLocation,
    ListInstances,
    InjectIntentRequest,
    SetDeveloperCertificate,
    ShowDevelopmentMode,
};

int main(int argc, char *argv[])
{
    ensureLibDBusIsAvailable(); // this needs to happen before the QCoreApplication constructor

    ToolApplication<Command> tool("Controller", argc, argv);

    tool.setCommands({
        { StartApplication,          "start-application",           "Start an application." },
        { DebugApplication,          "debug-application",           "Debug an application." },
        { StopApplication,           "stop-application",            "Stop an application." },
        { StopAllApplications,       "stop-all-applications",       "Stop all applications." },
        { ListApplications,          "list-applications",           "List all installed applications." },
        { ShowApplication,           "show-application",            "Show application meta-data." },
        { ListPackages,              "list-packages",               "List all installed packages." },
        { ShowPackage,               "show-package",                "Show package meta-data." },
        { InstallPackage,            "install-package",             "Install a package." },
        { RemovePackage,             "remove-package",              "Remove a package." },
        { ListInstallationTasks,     "list-installation-tasks",     "List all active installation tasks." },
        { CancelInstallationTask,    "cancel-installation-task",    "Cancel an active installation task." },
        { ShowInstallationLocation,  "show-installation-location",  "Show details for installation location." },
        { ListInstances,             "list-instances",              "List all named application manager instances." },
        { InjectIntentRequest,       "inject-intent-request",       "Inject an intent request for testing." },
        { SetDeveloperCertificate,   "set-developer-certificate",   "Set the developer certificate for development mode." },
        { ShowDevelopmentMode,       "show-development-mode",       "Show the current development mode status." },
    });

    QCommandLineParser clp;
    clp.addOption({ { u"instance-id"_s }, u"Connect to the named instance."_s, u"instance-id"_s });
    Command cmd = tool.parse(clp);

    // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
    try {
        if ((cmd != NoCommand) && (cmd != ListInstances) && !clp.isSet(u"help"_s))
            dbus()->setInstanceInfo(resolveInstanceInfo(clp.value(u"instance-id"_s)));

        switch (cmd) {
        case NoCommand:
            break;

        case StartApplication: {
            clp.addOption({ { u"i"_s, u"attach-stdin"_s }, u"Attach the app's stdin to the controller's stdin"_s });
            clp.addOption({ { u"o"_s, u"attach-stdout"_s }, u"Attach the app's stdout to the controller's stdout"_s });
            clp.addOption({ { u"e"_s, u"attach-stderr"_s }, u"Attach the app's stderr to the controller's stderr"_s });
            clp.addOption({ { u"r"_s, u"restart"_s }, u"Before starting, stop the application if it is already running"_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.addPositionalArgument(u"document-url"_s,   u"The optional document-url."_s, u"[document-url]"_s);
            clp.process(tool);

            int args = int(clp.positionalArguments().size());
            if ((args < 2) || (args > 3))
                clp.showHelp(1);

            QMap<QString, int> stdRedirections;
            if (clp.isSet(u"attach-stdin"_s))
                stdRedirections[u"in"_s] = 0;
            if (clp.isSet(u"attach-stdout"_s))
                stdRedirections[u"out"_s] = 1;
            if (clp.isSet(u"attach-stderr"_s))
                stdRedirections[u"err"_s] = 2;
            bool restart = clp.isSet(u"restart"_s);

            const QString debugWrapper = { };
            const QString appId = clp.positionalArguments().at(1);
            const QString documentUrl = (args == 3) ? clp.positionalArguments().at(2) : QString();

            tool.runLater([=] {
                startOrDebugApplication(debugWrapper, appId, stdRedirections, restart, documentUrl);
            });
            break;
        }
        case DebugApplication: {
            clp.addOption({ { u"i"_s, u"attach-stdin"_s }, u"Attach the app's stdin to the controller's stdin"_s });
            clp.addOption({ { u"o"_s, u"attach-stdout"_s }, u"Attach the app's stdout to the controller's stdout"_s });
            clp.addOption({ { u"e"_s, u"attach-stderr"_s }, u"Attach the app's stderr to the controller's stderr"_s });
            clp.addOption({ { u"r"_s, u"restart"_s }, u"Before starting, stop the application if it is already running"_s });
            clp.addPositionalArgument(u"debug-wrapper"_s,  u"The debug-wrapper specification."_s);
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.addPositionalArgument(u"document-url"_s,   u"The optional document-url."_s, u"[document-url]"_s);
            clp.process(tool);

            int args = int(clp.positionalArguments().size());
            if ((args < 3) || (args > 4))
                clp.showHelp(1);

            QMap<QString, int> stdRedirections;
            if (clp.isSet(u"attach-stdin"_s))
                stdRedirections[u"in"_s] = 0;
            if (clp.isSet(u"attach-stdout"_s))
                stdRedirections[u"out"_s] = 1;
            if (clp.isSet(u"attach-stderr"_s))
                stdRedirections[u"err"_s] = 2;
            bool restart = clp.isSet(u"restart"_s);

            const QString debugWrapper = clp.positionalArguments().at(1);
            const QString appId = clp.positionalArguments().at(2);
            const QString documentUrl = (args == 4) ? clp.positionalArguments().at(3) : QString();

            tool.runLater([=] {
                startOrDebugApplication(debugWrapper, appId, stdRedirections, restart, documentUrl);
            });
            break;
        }
        case StopAllApplications:
            clp.process(tool);
            if (clp.positionalArguments().size() != 1)
                clp.showHelp(1);

            tool.runLater(stopAllApplications);
            break;

        case StopApplication: {
            clp.addOption({ { u"f"_s, u"force"_s }, u"Force kill the application."_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            const QString appId = clp.positionalArguments().at(1);
            const bool force = clp.isSet(u"f"_s);

            tool.runLater([=] { stopApplication(appId, force); });
            break;
        }
        case ListApplications:
            clp.process(tool);
            tool.runLater(listApplications);
            break;

        case ShowApplication: {
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            const QString appId = clp.positionalArguments().at(1);
            const bool json = clp.isSet(u"json"_s);

            tool.runLater([=] { showApplication(appId, json); });
            break;
        }
        case ListPackages:
            clp.process(tool);
            tool.runLater(listPackages);
            break;

        case ShowPackage: {
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"package-id"_s, u"The id of an installed package."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            const QString appId = clp.positionalArguments().at(1);
            const bool json = clp.isSet(u"json"_s);

            tool.runLater([=] { showPackage(appId, json); });
            break;
        }
        case InstallPackage: {
            clp.addOption({ { u"l"_s, u"location"_s }, u"Set a custom installation location (deprecated and ignored)."_s, u"installation-location"_s, u"internal-0"_s });
            clp.addOption({ { u"a"_s, u"acknowledge"_s }, u"Automatically acknowledge the installation (unattended mode)."_s });
            clp.addPositionalArgument(u"package"_s, u"The file name of the package; can be - for stdin."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);
            if (clp.isSet(u"l"_s))
                std::cerr << "Ignoring the deprecated -l option.\n";

            const QString package = clp.positionalArguments().at(1);
            const bool acknowledge = clp.isSet(u"a"_s);

            tool.runLater([=] { installPackage(package, acknowledge); });
            break;
        }
        case RemovePackage: {
            clp.addOption({ { u"f"_s, u"force"_s }, u"Force removal of package."_s });
            clp.addOption({ { u"k"_s, u"keep-documents"_s }, u"Keep the document folder of the application."_s });
            clp.addPositionalArgument(u"package-id"_s, u"The id of an installed package."_s);
            clp.process(tool);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            const QString packageId = clp.positionalArguments().at(1);
            const bool force = clp.isSet(u"f"_s);
            const bool keepDocuments = clp.isSet(u"k"_s);

            tool.runLater([=] { removePackage(packageId, keepDocuments, force); });
            break;
        }
        case ListInstallationTasks:
            clp.process(tool);
            tool.runLater(listInstallationTasks);
            break;

        case CancelInstallationTask: {
            clp.addPositionalArgument(u"task-id"_s, u"The id of an active installation task."_s);
            clp.addOption({ { u"a"_s, u"all"_s }, u"Cancel all active installation tasks."_s });
            clp.process(tool);

            qsizetype args = clp.positionalArguments().size();
            bool all = clp.isSet(u"a"_s);
            if (!(((args == 1) && all) || ((args == 2) && !all)))
                clp.showHelp(1);

            const QString taskId = (args == 2) ? clp.positionalArguments().at(1) : QString();

            tool.runLater([=] { cancelInstallationTask(all, taskId); });
            break;
        }
        case ShowInstallationLocation: {
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.process(tool);

            if (clp.positionalArguments().size() > 1)
                clp.showHelp(1);

            const bool json = clp.isSet(u"json"_s);

            tool.runLater([=] { showInstallationLocation(json); });
            break;
        }
        case ListInstances:
            clp.process(tool);
            tool.runLater(listInstances);
            break;

        case InjectIntentRequest: {
            clp.addPositionalArgument(u"intent-id"_s, u"The id of the intent."_s);
            clp.addPositionalArgument(u"parameters"_s, u"The optional parameters for this request."_s, u"[json-parameters]"_s);
            clp.addOption({ u"requesting-application-id"_s, u"Fake the requesting application id."_s, u"id"_s, u":sysui:"_s });
            clp.addOption({ u"application-id"_s, u"Specify the handling application id."_s, u"id"_s });
            clp.addOption({ u"broadcast"_s, u"Create a broadcast request."_s });
            clp.process(tool);

            bool isBroadcast = clp.isSet(u"broadcast"_s);
            QString appId = clp.value(u"application-id"_s);
            QString requestingAppId = clp.value(u"requesting-application-id"_s);

            if (!appId.isEmpty() && isBroadcast)
                throw Exception("You cannot use --application-id and --broadcast at the same time.");

            if (clp.positionalArguments().size() < 2)
                clp.showHelp(1);

            if (clp.positionalArguments().size() > 3)
                clp.showHelp(1);

            QString jsonParams;
            if (clp.positionalArguments().size() == 3)
                jsonParams = clp.positionalArguments().at(2);

            const QString intentId = clp.positionalArguments().at(1);

            tool.runLater([=] {
                injectIntentRequest(intentId, isBroadcast, requestingAppId, appId, jsonParams);
            });
            break;
        }
        case SetDeveloperCertificate: {
            clp.addPositionalArgument(u"certificate"_s, u"PKCS#12 certificate file."_s);
            clp.addOption({ u"clear"_s, u"Remove the currently set developer certificate."_s });
            clp.addOption({{ u"p"_s, u"password"_s },
                           u"Password for the PKCS#12 certificate in the form "
                           "pass:<password>, env:<envvar>, file:<path>, fd:<number> or stdin. "
                           "See the documentation for details."_s,
                           u"format[:value]"_s });
            clp.process(tool);

            const bool isClear = clp.isSet(u"clear"_s);
            const bool hasPassword = clp.isSet(u"p"_s);

            if (clp.positionalArguments().size() != (isClear ? 1 : 2))
                clp.showHelp(1);

            if (isClear && hasPassword)
                throw Exception("Cannot use --password and --clear at the same time.");

            const QString certificate = isClear ? QString() : clp.positionalArguments().at(1);
            QString password = clp.value(u"p"_s);

            if ((certificate == u"-") && (password == u"stdin"))
                throw Exception("Cannot read both the certificate and the password from stdin");

            password = tool.parsePasswordOption(password, u"PKCS#12 certificate password"_s);

            tool.runLater([=] { setDeveloperCertificate(certificate, password, isClear); });
            break;
        }
        case ShowDevelopmentMode:
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.process(tool);

            if (clp.positionalArguments().size() > 1)
                clp.showHelp(1);

            const bool json = clp.isSet(u"json"_s);

            tool.runLater([=] { showDevelopmentMode(json); });
            break;
        }

        return tool.exec();

    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 2;
    }
}

void startOrDebugApplication(const QString &debugWrapper, const QString &appId,
                             const QMap<QString, int> &stdRedirections, bool restart,
                             const QString &documentUrl = QString()) noexcept(false)
{
    dbus()->connectToManager();

    if (restart) {
        bool isStopped = false;

        // pass 0: normal stop / pass 1: force kill
        for (int pass = 0; !isStopped && (pass < 2); ++pass) {

            auto stopReply = dbus()->manager()->stopApplication(appId, pass > 0 /*forceKill*/);
            stopReply.waitForFinished();
            if (stopReply.isError())
                throw Exception(Error::IO, "failed to call stopApplication via DBus: %1").arg(stopReply.error().message());

            static const int checksPerSecond = 10;

            // check if application has quit for max. 3sec
            for (int i = 0; !isStopped && (i < (3 * checksPerSecond)); ++i) {
                auto stateReply = dbus()->manager()->applicationRunState(appId);
                stateReply.waitForFinished();
                if (stateReply.isError())
                    throw Exception(Error::IO, "failed to get the current run-state from application manager: %1").arg(stateReply.error().message());

                if (stateReply.value() == 0 /* NotRunning */)
                    isStopped = true;
                else
                    QThread::currentThread()->msleep(1000 / checksPerSecond);
            }
        }

        if (!isStopped)
            throw Exception("failed to stop application %1 before restarting it").arg(appId);
    }

    // the async lambda below needs to share this variable
    static bool isStarted = false;

    if (!stdRedirections.isEmpty()) {
        // just bail out, if the AM or bus dies
        QObject::connect(dbus(), &DBus::disconnected,
                         qApp, [](const QString &reason) {
            throw Exception(Error::IO, "application might not be running: lost connection to the D-Bus service (%1)").arg(reason);
        });

        // in case application quits -> quit the controller
        QObject::connect(dbus()->manager(), &IoQtApplicationManagerInterface::applicationRunStateChanged,
                         qApp, [appId](const QString &id, uint runState) {
            if (isStarted && id == appId && runState == 0 /* NotRunning */) {
                auto getReply = dbus()->manager()->get(id);
                getReply.waitForFinished();
                if (getReply.isError())
                    throw Exception(Error::IO, "failed to get exit code from application manager: %1").arg(getReply.error().message());
                std::cout << "\n --- application has quit ---\n\n";
                auto app = getReply.value();
                qApp->exit(app.value(u"lastExitCode"_s, 1).toInt());
            }
        });

        // Workaround for a race condition in QtDBus, where the bus sometimes disconnects, if the
        // first thing you do after a successful connect to the peer is to send file descriptors
        bool b = dbus()->manager()->singleProcess();
        Q_UNUSED(b);
    }

    bool isDebug = !debugWrapper.isEmpty();
    bool hasRedirections = !stdRedirections.isEmpty();
#if defined(Q_OS_WINDOWS)
    if (hasRedirections) {
        std::cerr << "WARNING: Ignoring std-in/out/err redirections, as these are not supported on Windows."
                  << std::endl;
        hasRedirections = false;
    }
#endif
    QDBusPendingReply<bool> reply;

    if (!hasRedirections) {
        reply = isDebug ? dbus()->manager()->debugApplication(appId, debugWrapper, documentUrl)
                        : dbus()->manager()->startApplication(appId, documentUrl);
    } else {
        QMap<QString, QDBusUnixFileDescriptor> fdMap;
        for (auto it = stdRedirections.cbegin(); it != stdRedirections.cend(); ++it)
            fdMap.insert(it.key(), QDBusUnixFileDescriptor(it.value()));

        reply = isDebug ? dbus()->manager()->debugApplication(appId, debugWrapper, fdMap, documentUrl)
                        : dbus()->manager()->startApplication(appId, fdMap, documentUrl);
    }

    reply.waitForFinished();
    if (reply.isError()) {
        throw Exception(Error::IO, "failed to call %2Application via DBus: %1")
                .arg(reply.error().message()).arg(isDebug ? "debug" : "start");
    }

    isStarted = reply.value();
    if (stdRedirections.isEmpty() || !isStarted) {
        qApp->exit(isStarted ? 0 : 2);
    } else {
        installInterruptHandler([appId](int sig) {
            std::cout << "Stopping application due to signal " << UnixSignalHandler::signalName(sig)
                      << ".\n";
            auto stopReply = dbus()->manager()->stopApplication(appId, true);
            stopReply.waitForFinished();
            qApp->exit(1);
        });
    }
}

void stopApplication(const QString &appId, bool forceKill) noexcept(false)
{
    dbus()->connectToManager();

    auto reply = dbus()->manager()->stopApplication(appId, forceKill);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call stopApplication via DBus: %1").arg(reply.error().message());
    qApp->quit();
}

void stopAllApplications() noexcept(false)
{
    dbus()->connectToManager();

    auto reply = dbus()->manager()->stopAllApplications();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call stopAllApplications via DBus: %1").arg(reply.error().message());
    qApp->quit();
}

void listApplications() noexcept(false)
{
    dbus()->connectToManager();

    auto reply = dbus()->manager()->applicationIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call applicationIds via DBus: %1").arg(reply.error().message());

    const auto applicationIds = reply.value();
    for (const auto &applicationId : applicationIds)
        std::cout << qPrintable(applicationId) << '\n';
    qApp->quit();
}

void showApplication(const QString &appId, bool asJson) noexcept(false)
{
    dbus()->connectToManager();

    auto reply = dbus()->manager()->get(appId);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to get application via DBus: %1").arg(reply.error().message());

    QVariant app = convertFromDBusVariant(reply.value());
    std::cout << (asJson ? QJsonDocument::fromVariant(app).toJson().constData()
                         : YamlEmitter::fromVariantDocuments({ app }).constData()) << '\n';
    qApp->quit();
}

void listPackages() noexcept(false)
{
    dbus()->connectToPackager();

    auto reply = dbus()->packager()->packageIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call packageIds via DBus: %1").arg(reply.error().message());

    const auto packageIds = reply.value();
    for (const auto &packageId : packageIds)
        std::cout << qPrintable(packageId) << '\n';
    qApp->quit();
}

void showPackage(const QString &packageId, bool asJson) noexcept(false)
{
    dbus()->connectToPackager();

    auto reply = dbus()->packager()->get(packageId);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to get package via DBus: %1").arg(reply.error().message());

    QVariant package = convertFromDBusVariant(reply.value());
    std::cout << (asJson ? QJsonDocument::fromVariant(package).toJson().constData()
                                   : YamlEmitter::fromVariantDocuments({ package }).constData()) << '\n';
    qApp->quit();
}

void installPackage(const QString &package, bool acknowledge) noexcept(false)
{
    QString packageFile = package;

    if (package == u"-") { // sent via stdin
        bool success = false;

        auto *tf = new QTemporaryFile(qApp);
        QFile in;

        if (tf->open() && in.open(stdin, QIODevice::ReadOnly)) {
            packageFile = tf->fileName();

            while (!in.atEnd() && !tf->error())
                tf->write(in.read(1024 * 1024 * 8));

            success = in.atEnd() && !tf->error();
            tf->flush();
        }

        if (!success)
            throw Exception(Error::IO, "Could not copy from stdin to temporary file %1").arg(package);
    }

    if (packageFile.startsWith(u"file://"))
        packageFile = QUrl(packageFile).toLocalFile();
    if (!packageFile.startsWith(u"http://") && !packageFile.startsWith(u"https://")) {
        QFileInfo fi(packageFile);
        if (!fi.exists() || !fi.isReadable() || !fi.isFile())
            throw Exception(Error::IO, "Package file is not readable: %1").arg(packageFile);
        packageFile = fi.absoluteFilePath();
    }

    std::cout << "Starting installation of package " << qPrintable(packageFile) << "...\n";

    dbus()->connectToManager();
    dbus()->connectToPackager();

    // just bail out, if the AM or bus dies
    QObject::connect(dbus(), &DBus::disconnected,
                     qApp, [](const QString &reason) {
        throw Exception(Error::IO, "package might not be installed: lost connection to the D-Bus service (%1)").arg(reason);
    });

    // all the async lambdas below need to share this variable
    static QString installationId;

    // as soon as we have the manifest available: get the app id and acknowledge the installation
    if (acknowledge) {
        QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskRequestingInstallationAcknowledge,
                         qApp, [](const QString &taskId, const QVariantMap &metadata) {
            if (taskId != installationId)
                return;
            QString packageId = metadata.value(u"packageId"_s).toString();
            if (packageId.isEmpty())
                throw Exception(Error::IO, "could not find a valid package id in the package");
            std::cout << "Acknowledging package installation for " << qPrintable(packageId) << "...\n";
            dbus()->packager()->acknowledgePackageInstallation(taskId);
        });
    }

    // on failure: quit
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFailed,
                     qApp, [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to install package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFinished,
                     qApp, [](const QString &taskId) {
        if (taskId != installationId)
            return;
        std::cout << "Package installation finished successfully.\n";
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus()->packager()->startPackageInstallation(packageFile);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call startPackageInstallation via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "startPackageInstallation returned an empty taskId");

    // cancel the job on Ctrl+C

    installInterruptHandler([](int sig) {
        std::cout << "Cancelling package installation due to signal "
                  << UnixSignalHandler::signalName(sig) << ".\n";
        auto cancelReply = dbus()->packager()->cancelTask(installationId);
        cancelReply.waitForFinished();
        qApp->exit(1);
    });
}

void removePackage(const QString &packageId, bool keepDocuments, bool force) noexcept(false)
{
    std::cout << "Starting removal of package " << qPrintable(packageId) << "...\n";

    dbus()->connectToManager();
    dbus()->connectToPackager();

    // just bail out, if the AM or bus dies
    QObject::connect(dbus(), &DBus::disconnected,
                     qApp, [](const QString &reason) {
        throw Exception(Error::IO, "package might not be removed: lost connection to the D-Bus service (%1)").arg(reason);
    });

    // both the async lambdas below need to share this variables
    static QString installationId;

    // on failure: quit
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFailed,
                     qApp, [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to remove package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFinished,
                     qApp, [](const QString &taskId) {
        if (taskId != installationId)
            return;
        std::cout << "Package removal finished successfully.\n";
        qApp->quit();
    });

    // start the package removal
    auto reply = dbus()->packager()->removePackage(packageId, keepDocuments, force);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call removePackage via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "removePackage returned an empty taskId");
}

void listInstallationTasks() noexcept(false)
{
    dbus()->connectToPackager();

    auto reply = dbus()->packager()->activeTaskIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call activeTaskIds via DBus: %1").arg(reply.error().message());

    const auto taskIds = reply.value();
    for (const auto &taskId : taskIds)
        std::cout << qPrintable(taskId) << '\n';
    qApp->quit();
}


void cancelInstallationTask(bool all, const QString &singleTaskId) noexcept(false)
{
    dbus()->connectToPackager();

    // just bail out, if the AM or bus dies
    QObject::connect(dbus(), &DBus::disconnected,
                     qApp, [](const QString &reason) {
        throw Exception(Error::IO, "installation task(s) might not be canceled: lost connection to the D-Bus service (%1)").arg(reason);
    });

    // both the async lambdas below need to share these variables
    static QStringList cancelTaskIds;
    static int result = 0;

    if (all) {
        dbus()->connectToPackager();

        auto reply = dbus()->packager()->activeTaskIds();
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call activeTaskIds via DBus: %1").arg(reply.error().message());

        const auto taskIds = reply.value();
        cancelTaskIds.reserve(taskIds.size());
        for (const auto &taskId : taskIds)
            cancelTaskIds << taskId;
    } else {
        cancelTaskIds << singleTaskId;
    }

    if (cancelTaskIds.isEmpty())
        qApp->quit();

    // on task failure
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFailed,
                     qApp, [](const QString &taskId, int errorCode, const QString &errorString) {
        if (cancelTaskIds.removeOne(taskId)) {
            if (errorCode != int(Error::Canceled)) {
                std::cout << "Could not cancel task " << qPrintable(taskId) << ": "
                          << "the installation task already failed (" << qPrintable(errorString)
                          << ").\n";
                result |= 2;
            } else {
                std::cout << "Installation task was canceled successfully.\n";
            }
            if (cancelTaskIds.isEmpty())
                qApp->exit(result);
        }
    });

    // on success
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFinished,
                     qApp, [](const QString &taskId) {
        if (cancelTaskIds.removeOne(taskId)) {
            std::cout << "Could not cancel task " << qPrintable(taskId) << ": "
                      << "the installation task already finished successfully.\n",
            result |= 1;
            if (cancelTaskIds.isEmpty())
                qApp->exit(result);
        }
    });

    for (const auto &cancelTaskId : std::as_const(cancelTaskIds)) {
        std::cout << "Canceling installation task " << qPrintable(cancelTaskId) << "...\n";

        // cancel the task

        auto reply = dbus()->packager()->cancelTask(cancelTaskId);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call cancelTask via DBus: %1").arg(reply.error().message());

        if (!reply.value())
            throw Exception(Error::IO, "failed to cancel the installation task.");
    }
}

void showInstallationLocation(bool asJson) noexcept(false)
{
    dbus()->connectToPackager();

    auto location = dbus()->packager()->installationLocation();
    dbus()->throwOnError();

    std::cout << (asJson ? QJsonDocument::fromVariant(location).toJson().constData()
                         : YamlEmitter::fromVariantDocuments({ location }).constData()) << '\n';
    qApp->quit();
}

static std::pair<QString, QMultiHash<QString, int>> runningInstanceIds()
{
    QMultiHash<QString, int> result;

    QString rtPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtPath.isEmpty())
        rtPath = QDir::tempPath();
    QDir rtDir(rtPath);
    if (!rtDir.cd(u"qtapplicationmanager"_s))
        return { rtDir.path(), result };

    const QString suffix = u".lock"_s;
    QDirIterator dit(rtDir.path(), { u'*' + suffix });
    while (dit.hasNext()) {
        QString path = dit.next();
        QString name = dit.fileName();
        name.chop(suffix.length());
        if (auto dashPos = name.lastIndexOf(u'-'); dashPos > 0) {
            bool counterOk = false;
            int counter = QStringView { name }.sliced(dashPos + 1).toInt(&counterOk);
            if (counterOk) {
                QLockFile testLock(path);
                if (testLock.tryLock(0))
                    testLock.unlock(); // stale lock
                else if (testLock.error() != QLockFile::LockFailedError) {
                    std::cerr << "WARNING: unrecoverable, stale lock file: " << qPrintable(path)
                              << std::endl;
                } else {
                    result.insert(name.left(dashPos), counter);
                }
            }
        }
    }
    return { rtDir.path(), result };
}

static QVariantMap resolveInstanceInfo(const QString &instanceId)
{
    static const QString defaultInstanceId = u"appman"_s;
    static QRegularExpression re(uR"(^(.+?)(?:-(\d+))?$)"_s);

    const auto [baseDir, running] = runningInstanceIds();
    QString iid = instanceId.isEmpty() ? defaultInstanceId : instanceId;
    QString result;

    try {
        QString id;
        int counter = -1;
        auto m = re.match(iid);
        if (!m.hasMatch())
            throw Exception("Invalid instance-id");
        id = m.captured(1);
        bool counterOk = true;
        counter = m.hasCaptured(2) ? int(m.captured(2).toUInt(&counterOk)) : -1;
        if (!counterOk)
            throw Exception("Invalid instance-id");

        if (counter >= 0) {
            // fully qualified instance id: must match exactly
            if (running.contains(id, counter))
                result = instanceId;
        } else if (running.count(id) == 1) {
            // id only: matches if there's exactly one instance with that name
            result = id + u'-' + QString::number(running[id]);
        } else if (instanceId.isEmpty() && (running.count(id) == 0)
                   && (running.count() == 1)) {
            // no id: matches even a named instance, if that is the only instance running
            result = running.constBegin().key() + u'-' + QString::number(running.constBegin().value());
        }

        if (result.isEmpty()) {
            throw Exception("Could not resolve the given instance-id (%1) to any running appman instance")
                .arg(instanceId);
        }
    } catch (const Exception &e) {
        QStringList allIds;
        for (auto it = running.cbegin(); it != running.cend(); ++it)
            allIds.append(it.key() + u'-' + QString::number(it.value()));
        throw Exception(u"%1\n\nAvailable instances:\n  %2"_s.arg(e.errorString())
                        .arg(allIds.join(u"\n  ")));
    }

    QFile infof(baseDir + u'/' + result + u".json"_s);
    if (!infof.open(QIODevice::ReadOnly))
        throw Exception(infof, "Could not open instance info file");

    QJsonParseError jsonError;
    const auto json = QJsonDocument::fromJson(infof.readAll(), &jsonError);
    if (json.isNull()) {
        throw Exception("Failed to parse instance info file (%1) as JSON: %2")
            .arg(infof.fileName()).arg(jsonError.errorString());
    }
    return json.toVariant().toMap();
}

void listInstances()
{
    const auto [_, running] = runningInstanceIds();
    for (auto it = running.cbegin(); it != running.cend(); ++it) {
        auto &name = it.key();
        std::cout << qPrintable(name) << '-' << it.value() << '\n';
    }
    qApp->quit();
}

void injectIntentRequest(const QString &intentId, bool isBroadcast,
                         const QString &requestingApplicationId, const QString &applicationId,
                         const QString &jsonParameters) noexcept(false)
{
    dbus()->connectToManager();

    if (isBroadcast) {
        auto reply = dbus()->manager()->broadcastIntentRequestAs(requestingApplicationId,
                    intentId,
                    jsonParameters);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call broadcastIntentRequest via DBus: %1").arg(reply.error().message());
    } else {
        auto reply = dbus()->manager()->sendIntentRequestAs(requestingApplicationId,
                    intentId,
                    applicationId,
                    jsonParameters);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call sendIntentRequest via DBus: %1").arg(reply.error().message());
        const auto jsonResult = reply.value();
        std::cout << qPrintable(jsonResult) << '\n';
    }

    qApp->quit();
}

void setDeveloperCertificate(const QString &pkcs12Path, const QString &pkcs12Password, bool clear)
{
    dbus()->connectToPackager();

    QByteArray pkcs12Data;
    if (clear) {
        Q_ASSERT(pkcs12Path.isEmpty());
        Q_ASSERT(pkcs12Password.isEmpty());
    } else {
        QFile cf;
        bool isOpen = false;
        if (pkcs12Path == u"-") { // sent via stdin
            isOpen = cf.open(stdin, QIODevice::ReadOnly);
        } else {
            cf.setFileName(pkcs12Path);
            isOpen = cf.open(QIODevice::ReadOnly);
        }
        if (!isOpen)
            throw Exception(cf, "could not open certificate file");
        pkcs12Data = cf.readAll();
    }
    auto reply = dbus()->packager()->setDeveloperCertificate(pkcs12Data, pkcs12Password.toUtf8());
    reply.waitForFinished();
    if (reply.isError())
        throw Exception("failed to call setDeveloperCertificate via DBus: %1").arg(reply.error().message());

    qApp->exit(reply.value() ? 0 : 1);
}

void showDevelopmentMode(bool asJson)
{
    dbus()->connectToPackager();

    QString devMode = dbus()->packager()->developmentMode();
    dbus()->throwOnError();
    QVariant devCert = convertFromDBusVariant(dbus()->packager()->developerCertificate().variant());
    dbus()->throwOnError();

    QVariantMap out { { u"developmentMode"_s, devMode }, { u"developerCertificate"_s, devCert } };

    std::cout << (asJson ? QJsonDocument::fromVariant(out).toJson().constData()
                         : YamlEmitter::fromVariantDocuments({ out }).constData()) << '\n';
    qApp->quit();
}
