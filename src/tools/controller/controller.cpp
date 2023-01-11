// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>
#include <QDir>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusError>
#include <QMetaObject>
#include <QStringBuilder>

#include <functional>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/unixsignalhandler.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/dbus-utilities.h>

#include "applicationmanager_interface.h"
#include "packagemanager_interface.h"

#include "interrupthandler.h"

QT_USE_NAMESPACE_AM

class DBus : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    DBus()
    {
        registerDBusTypes();
    }

    void setInstanceId(const QString &instanceId)
    {
        m_instanceId = instanceId;
        if (!m_instanceId.isEmpty())
            m_instanceId.append(u'-');
    }

    void connectToManager() Q_DECL_NOEXCEPT_EXPR(false)
    {
        if (m_manager)
            return;

        auto conn = connectTo(qSL("io.qt.ApplicationManager"));
        m_manager = new IoQtApplicationManagerInterface(qSL("io.qt.ApplicationManager"), qSL("/ApplicationManager"), conn, this);
    }

    void connectToPackager() Q_DECL_NOEXCEPT_EXPR(false)
    {
        if (m_packager)
            return;

        auto conn = connectTo(qSL("io.qt.PackageManager"));
        m_packager = new IoQtPackageManagerInterface(qSL("io.qt.ApplicationManager"), qSL("/PackageManager"), conn, this);
    }

private:
    QDBusConnection connectTo(const QString &iface) Q_DECL_NOEXCEPT_EXPR(false)
    {
        QDBusConnection conn(iface);

        QFile f(QDir::temp().absoluteFilePath(m_instanceId % QString(qSL("%1.dbus")).arg(iface)));
        QString dbus;
        if (f.open(QFile::ReadOnly)) {
            dbus = QString::fromUtf8(f.readAll());
            if (dbus == qL1S("system")) {
                conn = QDBusConnection::systemBus();
                dbus = qSL("[system-bus]");
            } else if (dbus.isEmpty()) {
                conn = QDBusConnection::sessionBus();
                dbus = qSL("[session-bus]");
            } else {
                conn = QDBusConnection::connectToBus(dbus, qSL("custom"));
            }
        } else {
            throw Exception(Error::IO, "Could not find the D-Bus interface of a running application manager instance.\n(did you start the appman with '--dbus none'?");
        }

        if (!conn.isConnected()) {
            throw Exception(Error::IO, "Could not connect to the application manager D-Bus interface %1 at %2: %3")
                .arg(iface, dbus, conn.lastError().message());
        }
        return conn;
    }

public:
    IoQtPackageManagerInterface *packager() const
    {
        return m_packager;
    }

    IoQtApplicationManagerInterface *manager() const
    {
        return m_manager;
    }

private:
    IoQtPackageManagerInterface *m_packager = nullptr;
    IoQtApplicationManagerInterface *m_manager = nullptr;
    QString m_instanceId;
};

static class DBus dbus;


enum Command {
    NoCommand,
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
    ListInstallationLocations,
    ShowInstallationLocation,
    ListInstances,
    InjectIntentRequest,
};

// REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
static struct {
    Command command;
    const char *name;
    const char *description;
} commandTable[] = {
    { StartApplication, "start-application", "Start an application." },
    { DebugApplication, "debug-application", "Debug an application." },
    { StopApplication,  "stop-application",  "Stop an application." },
    { StopAllApplications,  "stop-all-applications",  "Stop all applications." },
    { ListApplications, "list-applications", "List all installed applications." },
    { ShowApplication,  "show-application",  "Show application meta-data." },
    { ListPackages,     "list-packages",     "List all installed packages." },
    { ShowPackage,      "show-package",      "Show package meta-data." },
    { InstallPackage,   "install-package",   "Install a package." },
    { RemovePackage,    "remove-package",    "Remove a package." },
    { ListInstallationTasks,     "list-installation-tasks",     "List all active installation tasks." },
    { CancelInstallationTask,    "cancel-installation-task",    "Cancel an active installation task." },
    { ListInstallationLocations, "list-installation-locations", "List all installaton locations." },
    { ShowInstallationLocation,  "show-installation-location",  "Show details for installation location." },
    { ListInstances,    "list-instances",    "List all named application manager instances." },
    { InjectIntentRequest,       "inject-intent-request",       "Inject an intent request for testing." },
};

static Command command(QCommandLineParser &clp)
{
    if (!clp.positionalArguments().isEmpty()) {
        QByteArray cmd = clp.positionalArguments().at(0).toLatin1();

        for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
            if (cmd == commandTable[i].name) {
                clp.clearPositionalArguments();
                clp.addPositionalArgument(qL1S(cmd), qL1S(commandTable[i].description), qL1S(cmd));
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
}

static void startOrDebugApplication(const QString &debugWrapper, const QString &appId,
                                    const QMap<QString, int> &stdRedirections, bool restart,
                                    const QString &documentUrl) Q_DECL_NOEXCEPT_EXPR(false);
static void stopApplication(const QString &appId, bool forceKill = false) Q_DECL_NOEXCEPT_EXPR(false);
static void stopAllApplications() Q_DECL_NOEXCEPT_EXPR(false);
static void listApplications() Q_DECL_NOEXCEPT_EXPR(false);
static void showApplication(const QString &appId, bool asJson = false) Q_DECL_NOEXCEPT_EXPR(false);
static void listPackages() Q_DECL_NOEXCEPT_EXPR(false);
static void showPackage(const QString &packageId, bool asJson = false) Q_DECL_NOEXCEPT_EXPR(false);
static void installPackage(const QString &packageUrl, bool acknowledge) Q_DECL_NOEXCEPT_EXPR(false);
static void removePackage(const QString &packageId, bool keepDocuments, bool force) Q_DECL_NOEXCEPT_EXPR(false);
static void listInstallationTasks() Q_DECL_NOEXCEPT_EXPR(false);
static void cancelInstallationTask(bool all, const QString &taskId) Q_DECL_NOEXCEPT_EXPR(false);
static void listInstallationLocations() Q_DECL_NOEXCEPT_EXPR(false);
static void showInstallationLocation(bool asJson = false) Q_DECL_NOEXCEPT_EXPR(false);
static void listInstances() Q_DECL_NOEXCEPT_EXPR(false);
static void injectIntentRequest(const QString &intentId, bool isBroadcast,
                                const QString &applicationId, const QString &requestingApplicationId,
                                const QString &jsonParameters) Q_DECL_NOEXCEPT_EXPR(false);


class ThrowingApplication : public QCoreApplication // clazy:exclude=missing-qobject-macro
{
public:
    ThrowingApplication(int &argc, char **argv)
        : QCoreApplication(argc, argv)
    { }

    Exception *exception() const
    {
        return m_exception;
    }

    template <typename T> void runLater(T slot)
    {
        // run the specified function as soon as the event loop is up and running
        QMetaObject::invokeMethod(this, slot, Qt::QueuedConnection);
    }

protected:
    bool notify(QObject *o, QEvent *e) override
    {
        try {
            return QCoreApplication::notify(o, e);
        } catch (const Exception &e) {
            m_exception = new Exception(e);
            exit(3);
            return true;
        }
    }
private:
    Exception *m_exception = nullptr;
};

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(qSL("Qt Application Manager Controller"));
    QCoreApplication::setOrganizationName(qSL("QtProject"));
    QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    ThrowingApplication a(argc, argv);

    QByteArray desc = "\n\nAvailable commands are:\n";
    uint longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += "  ";
        desc += commandTable[i].name;
        desc += QByteArray(1 + int(longestName - qstrlen(commandTable[i].name)), ' ');
        desc += commandTable[i].description;
        desc += '\n';
    }

    desc += "\nMore information about each command can be obtained by running\n" \
            " appman-controller <command> --help";

    QCommandLineParser clp;

    clp.addOption({ { qSL("instance-id") }, qSL("Connect to the named instance."), qSL("instance-id") });
    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(qSL("command"), qSL("The command to execute."));

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    // ignore the return value here, as we also accept options we don't know about yet.
    // If an option is really not accepted by a command, the comman specific parsing should report
    // this.
    clp.parse(QCoreApplication::arguments());
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    dbus.setInstanceId(clp.value(qSL("instance-id")));

    // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
    try {
        switch (command(clp)) {
        case NoCommand:
            if (clp.isSet(qSL("version")))
                clp.showVersion();

            clp.setApplicationDescription(qSL("\n") + QCoreApplication::applicationName() + qL1S(desc));
            if (clp.isSet(qSL("help")))
                clp.showHelp(0);
            clp.showHelp(1);
            break;

        case StartApplication: {
            clp.addOption({ { qSL("i"), qSL("attach-stdin") }, qSL("Attach the app's stdin to the controller's stdin") });
            clp.addOption({ { qSL("o"), qSL("attach-stdout") }, qSL("Attach the app's stdout to the controller's stdout") });
            clp.addOption({ { qSL("e"), qSL("attach-stderr") }, qSL("Attach the app's stderr to the controller's stderr") });
            clp.addOption({ { qSL("r"), qSL("restart") }, qSL("Before starting, stop the application if it is already running") });
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.addPositionalArgument(qSL("document-url"),   qSL("The optional document-url."), qSL("[document-url]"));
            clp.process(a);

            int args = clp.positionalArguments().size();
            if (args < 2 || args > 3)
                clp.showHelp(1);

            QMap<QString, int> stdRedirections;
            if (clp.isSet(qSL("attach-stdin")))
                stdRedirections[qSL("in")] = 0;
            if (clp.isSet(qSL("attach-stdout")))
                stdRedirections[qSL("out")] = 1;
            if (clp.isSet(qSL("attach-stderr")))
                stdRedirections[qSL("err")] = 2;
            bool restart = clp.isSet(qSL("restart"));

            a.runLater(std::bind(startOrDebugApplication,
                                 QString(),
                                 clp.positionalArguments().at(1),
                                 stdRedirections,
                                 restart,
                                 args == 3 ? clp.positionalArguments().at(2) : QString()));
            break;
        }
        case DebugApplication: {
            clp.addOption({ { qSL("i"), qSL("attach-stdin") }, qSL("Attach the app's stdin to the controller's stdin") });
            clp.addOption({ { qSL("o"), qSL("attach-stdout") }, qSL("Attach the app's stdout to the controller's stdout") });
            clp.addOption({ { qSL("e"), qSL("attach-stderr") }, qSL("Attach the app's stderr to the controller's stderr") });
            clp.addOption({ { qSL("r"), qSL("restart") }, qSL("Before starting, stop the application if it is already running") });
            clp.addPositionalArgument(qSL("debug-wrapper"),  qSL("The debug-wrapper specification."));
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.addPositionalArgument(qSL("document-url"),   qSL("The optional document-url."), qSL("[document-url]"));
            clp.process(a);

            int args = clp.positionalArguments().size();
            if (args < 3 || args > 4)
                clp.showHelp(1);

            QMap<QString, int> stdRedirections;
            if (clp.isSet(qSL("attach-stdin")))
                stdRedirections[qSL("in")] = 0;
            if (clp.isSet(qSL("attach-stdout")))
                stdRedirections[qSL("out")] = 1;
            if (clp.isSet(qSL("attach-stderr")))
                stdRedirections[qSL("err")] = 2;
            bool restart = clp.isSet(qSL("restart"));

            a.runLater(std::bind(startOrDebugApplication,
                                 clp.positionalArguments().at(1),
                                 clp.positionalArguments().at(2),
                                 stdRedirections,
                                 restart,
                                 args == 3 ? clp.positionalArguments().at(2) : QString()));
            break;
        }
        case StopAllApplications:
            clp.process(a);
            if (clp.positionalArguments().size() != 1)
                clp.showHelp(1);

            a.runLater(stopAllApplications);
            break;

        case StopApplication:
            clp.addOption({ { qSL("f"), qSL("force") }, qSL("Force kill the application.") });
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(stopApplication,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("f"))));
            break;

        case ListApplications:
            clp.process(a);
            a.runLater(listApplications);
            break;

        case ShowApplication:
            clp.addOption({ qSL("json"), qSL("Output in JSON format instead of YAML.") });
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(showApplication,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("json"))));
            break;

        case ListPackages:
            clp.process(a);
            a.runLater(listPackages);
            break;

        case ShowPackage:
            clp.addOption({ qSL("json"), qSL("Output in JSON format instead of YAML.") });
            clp.addPositionalArgument(qSL("package-id"), qSL("The id of an installed package."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(showPackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("json"))));
            break;

        case InstallPackage:
            clp.addOption({ { qSL("l"), qSL("location") }, qSL("Set a custom installation location (deprecated and ignored)."), qSL("installation-location"), qSL("internal-0") });
            clp.addOption({ { qSL("a"), qSL("acknowledge") }, qSL("Automatically acknowledge the installation (unattended mode).") });
            clp.addPositionalArgument(qSL("package"), qSL("The file name of the package; can be - for stdin."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);
            if (clp.isSet(qSL("l")))
                fprintf(stderr, "Ignoring the deprecated -l option.\n");

            a.runLater(std::bind(installPackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("a"))));
            break;

        case RemovePackage:
            clp.addOption({ { qSL("f"), qSL("force") }, qSL("Force removal of package.") });
            clp.addOption({ { qSL("k"), qSL("keep-documents") }, qSL("Keep the document folder of the application.") });
            clp.addPositionalArgument(qSL("package-id"), qSL("The id of an installed package."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(removePackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("k")),
                                 clp.isSet(qSL("f"))));
            break;

        case ListInstallationTasks:
            clp.process(a);
            a.runLater(listInstallationTasks);
            break;

        case CancelInstallationTask: {
            clp.addPositionalArgument(qSL("task-id"), qSL("The id of an active installation task."));
            clp.addOption({ { qSL("a"), qSL("all") }, qSL("Cancel all active installation tasks.") });
            clp.process(a);

            int args = clp.positionalArguments().size();
            bool all = clp.isSet(qSL("a"));
            if (!(((args == 1) && all) || ((args == 2) && !all)))
                clp.showHelp(1);

            a.runLater(std::bind(cancelInstallationTask,
                                 all,
                                 args == 2 ? clp.positionalArguments().at(1) : QString()));
            break;
        }
        case ListInstallationLocations:
            clp.process(a);
            a.runLater(listInstallationLocations);
            break;

        case ShowInstallationLocation:
            clp.addPositionalArgument(qSL("installation-location"), qSL("The id of an installation location (deprecated and ignored)."));
            clp.addOption({ qSL("json"), qSL("Output in JSON format instead of YAML.") });
            clp.process(a);

            if (clp.positionalArguments().size() > 2)
                clp.showHelp(1);
            if (clp.positionalArguments().size() == 2)
                fprintf(stderr, "Ignoring the deprecated installation-location.\n");

            a.runLater(std::bind(showInstallationLocation,
                                 clp.isSet(qSL("json"))));
            break;

        case ListInstances:
            clp.process(a);
            a.runLater(listInstances);
            break;

        case InjectIntentRequest:
            clp.addPositionalArgument(qSL("intent-id"), qSL("The id of the intent."));
            clp.addPositionalArgument(qSL("parameters"), qSL("The optional parameters for this request."), qSL("[json-parameters]"));
            clp.addOption({ qSL("requesting-application-id"), qSL("Fake the requesting application id."), qSL("id"), qSL(":sysui:") });
            clp.addOption({ qSL("application-id"), qSL("Specify the handling application id."), qSL("id") });
            clp.addOption({ qSL("broadcast"), qSL("Create a broadcast request.") });
            clp.process(a);

            bool isBroadcast = clp.isSet(qSL("broadcast"));
            QString appId = clp.value(qSL("application-id"));
            QString requestingAppId = clp.value(qSL("requesting-application-id"));

            if (!appId.isEmpty() && isBroadcast)
                throw Exception("You cannot use --application-id and --broadcast at the same time.");

            if (clp.positionalArguments().size() > 3)
                clp.showHelp(1);

            QString jsonParams;
            if (clp.positionalArguments().size() == 3)
                jsonParams = clp.positionalArguments().at(2);

            a.runLater(std::bind(injectIntentRequest, clp.positionalArguments().at(1),
                                 isBroadcast, requestingAppId, appId, jsonParams));
            break;
        }

        int result = a.exec();
        if (a.exception())
            throw *a.exception();
        return result;

    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
}

void startOrDebugApplication(const QString &debugWrapper, const QString &appId,
                             const QMap<QString, int> &stdRedirections, bool restart,
                             const QString &documentUrl = QString()) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    if (restart) {
        bool isStopped = false;

        // pass 0: normal stop / pass 1: force kill
        for (int pass = 0; !isStopped && (pass < 2); ++pass) {
            stopApplication(appId, pass == 0 ? false : true);

            static const int checksPerSecond = 10;

            // check if application has quit for max. 3sec
            for (int i = 0; !isStopped && (i < (3 * checksPerSecond)); ++i) {
                auto stateReply = dbus.manager()->applicationRunState(appId);
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
        // in case application quits -> quit the controller
        QObject::connect(dbus.manager(), &IoQtApplicationManagerInterface::applicationRunStateChanged,
                         qApp, [appId](const QString &id, uint runState) {
            if (isStarted && id == appId && runState == 0 /* NotRunning */) {
                auto getReply = dbus.manager()->get(id);
                getReply.waitForFinished();
                if (getReply.isError())
                    throw Exception(Error::IO, "failed to get exit code from application manager: %1").arg(getReply.error().message());
                fprintf(stdout, "\n --- application has quit ---\n\n");
                auto app = getReply.value();
                qApp->exit(app.value(qSL("lastExitCode"), 1).toInt());
            }
        });
    }

    bool isDebug = !debugWrapper.isEmpty();
    QDBusPendingReply<bool> reply;
    if (stdRedirections.isEmpty()) {
        reply = isDebug ? dbus.manager()->debugApplication(appId, debugWrapper, documentUrl)
                        : dbus.manager()->startApplication(appId, documentUrl);
    } else {
        UnixFdMap fdMap;
        for (auto it = stdRedirections.cbegin(); it != stdRedirections.cend(); ++it)
            fdMap.insert(it.key(), QDBusUnixFileDescriptor(it.value()));

        reply = isDebug ? dbus.manager()->debugApplication(appId, debugWrapper, fdMap, documentUrl)
                        : dbus.manager()->startApplication(appId, fdMap, documentUrl);
    }

    reply.waitForFinished();
    if (reply.isError()) {
        throw Exception(Error::IO, "failed to call %2Application via DBus: %1")
                .arg(reply.error().message()).arg(isDebug ? "debug" : "start");
    }

    isStarted = reply.value();
    if (stdRedirections.isEmpty()) {
        qApp->exit(isStarted ? 0 : 2);
    } else {
        if (!isStarted) {
            qApp->exit(2);
        } else {
            InterruptHandler::install([appId](int) {
                auto reply = dbus.manager()->stopApplication(appId, true);
                reply.waitForFinished();
                qApp->exit(1);
            });
        }
    }
}

void stopApplication(const QString &appId, bool forceKill) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    auto reply = dbus.manager()->stopApplication(appId, forceKill);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call stopApplication via DBus: %1").arg(reply.error().message());
    qApp->quit();
}

void stopAllApplications() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    auto reply = dbus.manager()->stopAllApplications();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call stopAllApplications via DBus: %1").arg(reply.error().message());
    qApp->quit();
}

void listApplications() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    auto reply = dbus.manager()->applicationIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call applicationIds via DBus: %1").arg(reply.error().message());

    const auto applicationIds = reply.value();
    for (const auto &applicationId : applicationIds)
        fprintf(stdout, "%s\n", qPrintable(applicationId));
    qApp->quit();
}

void showApplication(const QString &appId, bool asJson) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    auto reply = dbus.manager()->get(appId);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to get application via DBus: %1").arg(reply.error().message());

    QVariant app = reply.value();
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(app).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ app }).constData());
    qApp->quit();
}

void listPackages() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    auto reply = dbus.packager()->packageIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call packageIds via DBus: %1").arg(reply.error().message());

    const auto packageIds = reply.value();
    for (const auto &packageId : packageIds)
        fprintf(stdout, "%s\n", qPrintable(packageId));
    qApp->quit();
}

void showPackage(const QString &packageId, bool asJson) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    auto reply = dbus.packager()->get(packageId);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to get package via DBus: %1").arg(reply.error().message());

    QVariant package = reply.value();
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(package).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ package }).constData());
    qApp->quit();
}

void installPackage(const QString &package, bool acknowledge) Q_DECL_NOEXCEPT_EXPR(false)
{
    QString packageFile = package;

    if (package == qL1S("-")) { // sent via stdin
        bool success = false;

        QTemporaryFile *tf = new QTemporaryFile(qApp);
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

    QFileInfo fi(packageFile);
    if (!fi.exists() || !fi.isReadable() || !fi.isFile())
        throw Exception(Error::IO, "Package file is not readable: %1").arg(packageFile);

    fprintf(stdout, "Starting installation of package %s ...\n", qPrintable(packageFile));

    dbus.connectToManager();
    dbus.connectToPackager();

    // all the async lambdas below need to share this variable
    static QString installationId;

    // as soon as we have the manifest available: get the app id and acknowledge the installation

    if (acknowledge) {
        QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskRequestingInstallationAcknowledge,
                         [](const QString &taskId, const QVariantMap &metadata) {
            if (taskId != installationId)
                return;
            QString packageId = metadata.value(qSL("packageId")).toString();
            if (packageId.isEmpty())
                throw Exception(Error::IO, "could not find a valid package id in the package");
            fprintf(stdout, "Acknowledging package installation for '%s'...\n", qPrintable(packageId));
            dbus.packager()->acknowledgePackageInstallation(taskId);
        });
    }

    // on failure: quit

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to install package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFinished,
                     [](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Package installation finished successfully.\n");
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus.packager()->startPackageInstallation(fi.absoluteFilePath());
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call startPackageInstallation via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "startPackageInstallation returned an empty taskId");

    // cancel the job on Ctrl+C

    InterruptHandler::install([](int) {
        fprintf(stdout, "Cancelling package installation.\n");
        auto reply = dbus.packager()->cancelTask(installationId);
        reply.waitForFinished();
        qApp->exit(1);
    });
}

void removePackage(const QString &packageId, bool keepDocuments, bool force) Q_DECL_NOEXCEPT_EXPR(false)
{
    fprintf(stdout, "Starting removal of package %s...\n", qPrintable(packageId));

    dbus.connectToManager();
    dbus.connectToPackager();

    // both the async lambdas below need to share this variables
    static QString installationId;

    // on failure: quit

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to remove package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFinished,
                     [](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Package removal finished successfully.\n");
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus.packager()->removePackage(packageId, keepDocuments, force);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call removePackage via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "removePackage returned an empty taskId");
}

void listInstallationTasks() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    auto reply = dbus.packager()->activeTaskIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call activeTaskIds via DBus: %1").arg(reply.error().message());

    const auto taskIds = reply.value();
    for (const auto &taskId : taskIds)
        fprintf(stdout, "%s\n", qPrintable(taskId));
    qApp->quit();
}


void cancelInstallationTask(bool all, const QString &taskId) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    // both the async lambdas below need to share this variables
    static QStringList cancelTaskIds;
    static int result = 0;

    if (all) {
        dbus.connectToPackager();

        auto reply = dbus.packager()->activeTaskIds();
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call activeTaskIds via DBus: %1").arg(reply.error().message());

        const auto taskIds = reply.value();
        for (const auto &taskId : taskIds)
            cancelTaskIds << taskId;
    } else {
        cancelTaskIds << taskId;
    }

    if (cancelTaskIds.isEmpty())
        qApp->quit();

    // on task failure

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (cancelTaskIds.removeOne(taskId)) {
            if (errorCode != int(Error::Canceled)) {
                fprintf(stdout, "Could not cancel task %s anymore - the installation task already failed (%s).\n",
                        qPrintable(taskId), qPrintable(errorString));
                result |= 2;
            } else {
                fprintf(stdout, "Installation task was canceled successfully.\n");
            }
            if (cancelTaskIds.isEmpty())
                qApp->exit(result);
        }
    });

    // on success

    QObject::connect(dbus.packager(), &IoQtPackageManagerInterface::taskFinished,
                     [](const QString &taskId) {
        if (cancelTaskIds.removeOne(taskId)) {
            fprintf(stdout, "Could not cancel task %s anymore - the installation task already finished successfully.\n",
                    qPrintable(taskId));
            result |= 1;
            if (cancelTaskIds.isEmpty())
                qApp->exit(result);
        }
    });

    for (const auto &cancelTaskId : std::as_const(cancelTaskIds)) {
        fprintf(stdout, "Canceling installation task %s...\n", qPrintable(cancelTaskId));

        // cancel the task

        auto reply = dbus.packager()->cancelTask(cancelTaskId);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call cancelTask via DBus: %1").arg(reply.error().message());

        if (!reply.value())
            throw Exception(Error::IO, "failed to cancel the installation task.");
    }
}

void listInstallationLocations() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    auto installationLocation = dbus.packager()->installationLocation().variant().toMap();
    if (!installationLocation.isEmpty())
        fputs("internal-0\n", stdout);
    qApp->quit();
}

void showInstallationLocation(bool asJson) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToPackager();

    auto installationLocation = dbus.packager()->installationLocation().variant().toMap();
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(installationLocation).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ installationLocation }).constData());
    qApp->quit();
}

void listInstances()
{
    QString dir = QDir::temp().absolutePath() % u'/';
    QString suffix = qSL("io.qt.ApplicationManager.dbus");

    QDirIterator dit(dir, { u'*' % suffix });
    while (dit.hasNext()) {
        QByteArray name = dit.next().toLocal8Bit();
        name.chop(suffix.length());
        name = name.mid(dir.length());
        if (name.isEmpty()) {
            name = "(no instance id)";
        } else {
            name.chop(1); // remove the '-' separator
            name = '"' % name % '"';
        }
        fprintf(stdout, "%s\n", name.constData());
    }
    qApp->quit();
}

void injectIntentRequest(const QString &intentId, bool isBroadcast,
                         const QString &requestingApplicationId, const QString &applicationId,
                         const QString &jsonParameters) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToManager();

    if (isBroadcast) {
        auto reply = dbus.manager()->broadcastIntentRequestAs(requestingApplicationId,
                    intentId,
                    jsonParameters);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call broadcastIntentRequest via DBus: %1").arg(reply.error().message());
    } else {
        auto reply = dbus.manager()->sendIntentRequestAs(requestingApplicationId,
                    intentId,
                    applicationId,
                    jsonParameters);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call sendIntentRequest via DBus: %1").arg(reply.error().message());
        const auto jsonResult = reply.value();
        fprintf(stdout, "%s\n", qPrintable(jsonResult));
    }

    qApp->quit();
}
