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
#include <QDBusServiceWatcher>
#include <QDBusError>
#include <QMetaObject>
#include <QStringBuilder>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QLockFile>

#include <functional>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/unixsignalhandler.h>
#include <QtAppManCommon/utilities.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/dbus-utilities.h>

#include "applicationmanager_interface.h"
#include "packagemanager_interface.h"

#include "interrupthandler.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class DBus : public QObject
{
    Q_OBJECT

public:
    DBus()
        : m_dbusService(u"io.qt.ApplicationManager"_s)

    {
        registerDBusTypes();
    }

    void setInstanceInfo(const QVariantMap &instanceInfo)
    {
        m_dbusAddresses = instanceInfo[u"dbus"_s].toMap();
    }

    void connectToManager() noexcept(false)
    {
        if (m_manager)
            return;

        auto conn = connectTo(u"io.qt.ApplicationManager"_s);
        m_manager = tryConnectToDBusInterface<IoQtApplicationManagerInterface>(m_dbusService,
                                                                               u"/ApplicationManager"_s,
                                                                               conn.name(), this);
        if (!m_manager) {
            throw Exception("Could not connect to the io.qt.ApplicationManager D-Bus interface on %1")
                .arg(m_dbusName);
        }
    }

    void connectToPackager() noexcept(false)
    {
        if (m_packager)
            return;

        auto conn = connectTo(u"io.qt.PackageManager"_s);
        m_packager = tryConnectToDBusInterface<IoQtPackageManagerInterface>(m_dbusService,
                                                                            u"/PackageManager"_s,
                                                                            conn.name(), this);
        if (!m_packager) {
            throw Exception("Could not connect to the io.qt.PackageManager D-Bus interface on %1")
                .arg(m_dbusName);
        }
    }

    Q_SIGNAL void disconnected(QString reason);

private:
    template<typename T>
    static T *tryConnectToDBusInterface(const QString &service, const QString &path,
                                        const QString &connectionName, QObject *parent)
    {
        // we are working with very small delays in the milli-second range here, so a linear factor
        // to support valgrind would have to be very large and probably conflict with usage elsewhere
        // in the codebase, where the ranges are normally in the seconds.
        static const int timeout = timeoutFactor() * timeoutFactor();

        QDBusConnection conn(connectionName);

        if (!conn.isConnected())
            return nullptr;
        if (!service.isEmpty() && conn.interface()) {
            // the 'T' constructor can block up to 25sec (!), if the service is not registered!
            if (!conn.interface()->isServiceRegistered(service))
                return nullptr;
        }

        QElapsedTimer timer;
        timer.start();

        do {
            T *iface = new T(service, path, conn, parent);
            if (!iface->lastError().isValid())
                return iface;
            delete iface;
            QThread::msleep(static_cast<unsigned long>(timeout));
        } while (timer.elapsed() < (100 * timeout)); // 100msec base line

        return nullptr;
    }

    QDBusConnection connectTo(const QString &iface) noexcept(false)
    {
        QDBusConnection conn(iface);

        QString dbus = m_dbusAddresses.value(iface).toString();
        if (dbus == u"system") {
            conn = QDBusConnection::systemBus();
            m_dbusName = u"[system-bus]"_s;
        } else if (dbus.isEmpty()) {
            conn = QDBusConnection::sessionBus();
            m_dbusName = u"[session-bus]"_s;
        } else if (dbus.startsWith(u"p2p:")) {
            const auto address = dbus.mid(4);
            conn = QDBusConnection::connectToPeer(address, u"p2p"_s);
            m_dbusName = u"[p2p] "_s + address;
            m_dbusService.clear(); // no service names allowed on p2p busses
        } else {
            conn = QDBusConnection::connectToBus(dbus, u"custom"_s);
            m_dbusName = dbus;
        }

        if (!conn.isConnected()) {
            throw Exception(Error::IO, "Could not connect to the application manager D-Bus interface %1 at %2: %3")
                .arg(iface, m_dbusName, conn.lastError().message());
        }

        installDisconnectWatcher(conn, u"io.qt.ApplicationManager"_s);
        return conn;
    }

    void installDisconnectWatcher(const QDBusConnection &conn, const QString &serviceName)
    {
        if (m_disconnectedEmitted)
            return;

        if (!m_connections.contains(conn.name())) {
            auto *watcher = new QDBusServiceWatcher(serviceName, conn, QDBusServiceWatcher::WatchForOwnerChange, this);
            connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
                    this, [this](const QString &, const QString &, const QString &) {
                disconnectDetected(u"owner changed"_s);
            });
            m_connections.append(conn.name());
        }

        // serviceOwnerChanged does not work if the bus-daemon process dies (as is the case when
        // the AM starts its own session bus in --dbus=auto mode and then later crashes, killing
        // the bus-daemon with it).
        // QDBusConnection::isConnected() does not have a change signal, so we have to poll.
        if (!m_disconnectTimer) {
            m_disconnectTimer = new QTimer(this);
            connect(m_disconnectTimer, &QTimer::timeout, this, [this]() {
                for (const auto &name : std::as_const(m_connections)) {
                    if (!QDBusConnection(name).isConnected()) {
                        disconnectDetected(u"bus died"_s);
                        break;
                    }
                }
            });
            m_disconnectTimer->start(500);
        }
    }

    void disconnectDetected(const QString &reason)
    {
        if (!m_disconnectedEmitted) {
            emit disconnected(reason);
            m_disconnectedEmitted = true;
            if (m_disconnectTimer)
                m_disconnectTimer->stop();
        }
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
    QVariantMap m_dbusAddresses;
    QString m_dbusName;
    QString m_dbusService;
    QStringList m_connections;
    QTimer *m_disconnectTimer = nullptr;
    bool m_disconnectedEmitted = false;
};

Q_GLOBAL_STATIC(DBus, dbus)


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
                clp.addPositionalArgument(QString::fromLatin1(cmd),
                                          QString::fromLatin1(commandTable[i].description),
                                          QString::fromLatin1(cmd));
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
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
    QCoreApplication::setApplicationName(u"Qt Application Manager Controller"_s);
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(QString::fromLatin1(QT_AM_VERSION_STR));

    ensureLibDBusIsAvailable(); // this needs to happen before the QCoreApplication constructor

    ThrowingApplication a(argc, argv);

    QByteArray desc = "\n\nAvailable commands are:\n";
    size_t longestName = 0;
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

    clp.addOption({ { u"instance-id"_s }, u"Connect to the named instance."_s, u"instance-id"_s });
    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(u"command"_s, u"The command to execute."_s);

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    // ignore the return value here, as we also accept options we don't know about yet.
    // If an option is really not accepted by a command, the command specific parsing should report
    // this.
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    clp.parse(QCoreApplication::arguments());


    // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
    try {
        auto cmd = command(clp);
        if ((cmd != NoCommand) && (cmd != ListInstances))
            dbus()->setInstanceInfo(resolveInstanceInfo(clp.value(u"instance-id"_s)));

        switch (cmd) {
        case NoCommand:
            if (clp.isSet(u"version"_s))
                clp.showVersion();

            clp.setApplicationDescription(u"\n"_s + QCoreApplication::applicationName() + QString::fromLatin1(desc));
            if (clp.isSet(u"help"_s))
                clp.showHelp(0);
            clp.showHelp(1);
            break;

        case StartApplication: {
            clp.addOption({ { u"i"_s, u"attach-stdin"_s }, u"Attach the app's stdin to the controller's stdin"_s });
            clp.addOption({ { u"o"_s, u"attach-stdout"_s }, u"Attach the app's stdout to the controller's stdout"_s });
            clp.addOption({ { u"e"_s, u"attach-stderr"_s }, u"Attach the app's stderr to the controller's stderr"_s });
            clp.addOption({ { u"r"_s, u"restart"_s }, u"Before starting, stop the application if it is already running"_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.addPositionalArgument(u"document-url"_s,   u"The optional document-url."_s, u"[document-url]"_s);
            clp.process(a);

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

            a.runLater(std::bind(startOrDebugApplication,
                                 QString(),
                                 clp.positionalArguments().at(1),
                                 stdRedirections,
                                 restart,
                                 args == 3 ? clp.positionalArguments().at(2) : QString()));
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
            clp.process(a);

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
            clp.addOption({ { u"f"_s, u"force"_s }, u"Force kill the application."_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(stopApplication,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(u"f"_s)));
            break;

        case ListApplications:
            clp.process(a);
            a.runLater(listApplications);
            break;

        case ShowApplication:
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"application-id"_s, u"The id of an installed application."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(showApplication,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(u"json"_s)));
            break;

        case ListPackages:
            clp.process(a);
            a.runLater(listPackages);
            break;

        case ShowPackage:
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.addPositionalArgument(u"package-id"_s, u"The id of an installed package."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(showPackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(u"json"_s)));
            break;

        case InstallPackage:
            clp.addOption({ { u"l"_s, u"location"_s }, u"Set a custom installation location (deprecated and ignored)."_s, u"installation-location"_s, u"internal-0"_s });
            clp.addOption({ { u"a"_s, u"acknowledge"_s }, u"Automatically acknowledge the installation (unattended mode)."_s });
            clp.addPositionalArgument(u"package"_s, u"The file name of the package; can be - for stdin."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);
            if (clp.isSet(u"l"_s))
                fprintf(stderr, "Ignoring the deprecated -l option.\n");

            a.runLater(std::bind(installPackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(u"a"_s)));
            break;

        case RemovePackage:
            clp.addOption({ { u"f"_s, u"force"_s }, u"Force removal of package."_s });
            clp.addOption({ { u"k"_s, u"keep-documents"_s }, u"Keep the document folder of the application."_s });
            clp.addPositionalArgument(u"package-id"_s, u"The id of an installed package."_s);
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(removePackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(u"k"_s),
                                 clp.isSet(u"f"_s)));
            break;

        case ListInstallationTasks:
            clp.process(a);
            a.runLater(listInstallationTasks);
            break;

        case CancelInstallationTask: {
            clp.addPositionalArgument(u"task-id"_s, u"The id of an active installation task."_s);
            clp.addOption({ { u"a"_s, u"all"_s }, u"Cancel all active installation tasks."_s });
            clp.process(a);

            qsizetype args = clp.positionalArguments().size();
            bool all = clp.isSet(u"a"_s);
            if (!(((args == 1) && all) || ((args == 2) && !all)))
                clp.showHelp(1);

            a.runLater(std::bind(cancelInstallationTask,
                                 all,
                                 args == 2 ? clp.positionalArguments().at(1) : QString()));
            break;
        }
        case ShowInstallationLocation:
            clp.addOption({ u"json"_s, u"Output in JSON format instead of YAML."_s });
            clp.process(a);

            if (clp.positionalArguments().size() > 1)
                clp.showHelp(1);

            a.runLater(std::bind(showInstallationLocation,
                                 clp.isSet(u"json"_s)));
            break;

        case ListInstances:
            clp.process(a);
            a.runLater(listInstances);
            break;

        case InjectIntentRequest:
            clp.addPositionalArgument(u"intent-id"_s, u"The id of the intent."_s);
            clp.addPositionalArgument(u"parameters"_s, u"The optional parameters for this request."_s, u"[json-parameters]"_s);
            clp.addOption({ u"requesting-application-id"_s, u"Fake the requesting application id."_s, u"id"_s, u":sysui:"_s });
            clp.addOption({ u"application-id"_s, u"Specify the handling application id."_s, u"id"_s });
            clp.addOption({ u"broadcast"_s, u"Create a broadcast request."_s });
            clp.process(a);

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
        return int(e.errorCode());
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
                fprintf(stdout, "\n --- application has quit ---\n\n");
                auto app = getReply.value();
                qApp->exit(app.value(u"lastExitCode"_s, 1).toInt());
            }
        });
    }

    bool isDebug = !debugWrapper.isEmpty();
    bool hasRedirections = !stdRedirections.isEmpty();
#if defined(Q_OS_WINDOWS)
    if (hasRedirections) {
        fprintf(stderr, "WARNING: Ignoring std-in/out/err redirections, as these are not supported on Windows.");
        hasRedirections = false;
    }
#endif
    QDBusPendingReply<bool> reply;

    if (!hasRedirections) {
        reply = isDebug ? dbus()->manager()->debugApplication(appId, debugWrapper, documentUrl)
                        : dbus()->manager()->startApplication(appId, documentUrl);
    } else {
        UnixFdMap fdMap;
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
        InterruptHandler::install([appId](int) {
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
        fprintf(stdout, "%s\n", qPrintable(applicationId));
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
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(app).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ app }).constData());
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
        fprintf(stdout, "%s\n", qPrintable(packageId));
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
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(package).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ package }).constData());
    qApp->quit();
}

void installPackage(const QString &package, bool acknowledge) noexcept(false)
{
    QString packageFile = package;

    if (package == u"-") { // sent via stdin
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
            fprintf(stdout, "Acknowledging package installation for '%s'...\n", qPrintable(packageId));
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
        fprintf(stdout, "Package installation finished successfully.\n");
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus()->packager()->startPackageInstallation(fi.absoluteFilePath());
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call startPackageInstallation via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "startPackageInstallation returned an empty taskId");

    // cancel the job on Ctrl+C

    InterruptHandler::install([](int) {
        fprintf(stdout, "Cancelling package installation.\n");
        auto cancelReply = dbus()->packager()->cancelTask(installationId);
        cancelReply.waitForFinished();
        qApp->exit(1);
    });
}

void removePackage(const QString &packageId, bool keepDocuments, bool force) noexcept(false)
{
    fprintf(stdout, "Starting removal of package %s...\n", qPrintable(packageId));

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
        fprintf(stdout, "Package removal finished successfully.\n");
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
        fprintf(stdout, "%s\n", qPrintable(taskId));
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
    QObject::connect(dbus()->packager(), &IoQtPackageManagerInterface::taskFinished,
                     qApp, [](const QString &taskId) {
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

    auto installationLocation = dbus()->packager()->installationLocation();
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(installationLocation).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ installationLocation }).constData());
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
                else if (testLock.error() != QLockFile::LockFailedError)
                    fprintf(stderr, "WARNING: unrecoverable, stale lock file: %s\n", qPrintable(path));
                else
                    result.insert(name.left(dashPos), counter);
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
            throw Exception("Could not resolve the given instance-id (%1) to any running appman instance.\n  (did you start the appman with '--dbus none'?)")
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
        fprintf(stdout, "%s-%d\n", name.toLocal8Bit().constData(), it.value());
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
        fprintf(stdout, "%s\n", qPrintable(jsonResult));
    }

    qApp->quit();
}

#include "controller.moc"
