/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
#include <QTimer>
#include <QThread>

#if defined(Q_OS_UNIX)
#  include <sys/poll.h>
#endif

#include <functional>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/error.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/unixsignalhandler.h>
#include <QtAppManCommon/qtyaml.h>
#include <QtAppManCommon/dbus-utilities.h>

#include "applicationmanager_interface.h"
#include "applicationinstaller_interface.h"

QT_USE_NAMESPACE_AM

class DBus : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    DBus()
    {
        registerDBusTypes();
    }

    void connectToManager() Q_DECL_NOEXCEPT_EXPR(false)
    {
        if (m_manager)
            return;

        auto conn = connectTo(qSL("io.qt.ApplicationManager"));
        m_manager = new IoQtApplicationManagerInterface(qSL("io.qt.ApplicationManager"), qSL("/ApplicationManager"), conn, this);
    }

    void connectToInstaller() Q_DECL_NOEXCEPT_EXPR(false)
    {
        if (m_installer)
            return;

        auto conn = connectTo(qSL("io.qt.ApplicationInstaller"));
        m_installer = new IoQtApplicationInstallerInterface(qSL("io.qt.ApplicationManager"), qSL("/ApplicationInstaller"), conn, this);
    }

private:
    QDBusConnection connectTo(const QString &iface) Q_DECL_NOEXCEPT_EXPR(false)
    {
        QDBusConnection conn(iface);

        QFile f(QDir::temp().absoluteFilePath(QString(qSL("%1.dbus")).arg(iface)));
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
            throw Exception(Error::IO, "Could not find the D-Bus interface of a running Application-Manager instance.\n(did you start the appman with '--dbus none'?");
        }

        if (!conn.isConnected()) {
            throw Exception(Error::IO, "Could not connect to the Application-Manager D-Bus interface %1 at %2: %3")
                .arg(iface, dbus, conn.lastError().message());
        }
        return conn;
    }

public:
    IoQtApplicationInstallerInterface *installer() const
    {
        return m_installer;
    }

    IoQtApplicationManagerInterface *manager() const
    {
        return m_manager;
    }

private:
    IoQtApplicationInstallerInterface *m_installer = nullptr;
    IoQtApplicationManagerInterface *m_manager = nullptr;
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
    InstallPackage,
    RemovePackage,
    ListInstallationLocations,
    ShowInstallationLocation
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
    { InstallPackage,   "install-package",   "Install a package." },
    { RemovePackage,    "remove-package",    "Remove a package." },
    { ListInstallationLocations, "list-installation-locations", "List all installaton locations." },
    { ShowInstallationLocation,  "show-installation-location",  "Show details for installation location." }
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
static void installPackage(const QString &package, const QString &location, bool acknowledge) Q_DECL_NOEXCEPT_EXPR(false);
static void removePackage(const QString &package, bool keepDocuments, bool force) Q_DECL_NOEXCEPT_EXPR(false);
static void listInstallationLocations() Q_DECL_NOEXCEPT_EXPR(false);
static void showInstallationLocation(const QString &location, bool asJson = false) Q_DECL_NOEXCEPT_EXPR(false);

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
        QTimer::singleShot(0, this, slot);
    }

protected:
    bool notify(QObject *o, QEvent *e)
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
    QCoreApplication::setApplicationName(qSL("ApplicationManager Controller"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    ThrowingApplication a(argc, argv);

    QString desc = qSL("\nPelagicore ApplicationManager controller tool\n\nAvailable commands are:\n");
    uint longestName = 0;
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i)
        longestName = qMax(longestName, qstrlen(commandTable[i].name));
    for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
        desc += qSL("  %1%2  %3\n")
                .arg(qL1S(commandTable[i].name),
                     QString(int(longestName - qstrlen(commandTable[i].name)), qL1C(' ')),
                     qL1S(commandTable[i].description));
    }

    desc += qSL("\nMore information about each command can be obtained by running\n  appman-controller <command> --help");

    QCommandLineParser clp;
    clp.setApplicationDescription(desc);

    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(qSL("command"), qSL("The command to execute."));

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        exit(1);
    }
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    // REMEMBER to update the completion file util/bash/appman-prompt, if you apply changes below!
    try {
        switch (command(clp)) {
        case NoCommand:
            if (clp.isSet(qSL("version")))
                clp.showVersion();
            if (clp.isSet(qSL("help")))
                clp.showHelp();
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

        case InstallPackage:
            clp.addOption({ { qSL("l"), qSL("location") }, qSL("Set a custom installation location."), qSL("installation-location"), qSL("internal-0") });
            clp.addOption({ { qSL("a"), qSL("acknowledge") }, qSL("Automatically acknowledge the installation (unattended mode).") });
            clp.addPositionalArgument(qSL("package"), qSL("The file name of the package; can be - for stdin."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(installPackage,
                                 clp.positionalArguments().at(1),
                                 clp.value(qSL("l")),
                                 clp.isSet(qSL("a"))));
            break;

        case RemovePackage:
            clp.addOption({ { qSL("f"), qSL("force") }, qSL("Force removal of package.") });
            clp.addOption({ { qSL("k"), qSL("keep-documents") }, qSL("Keep the document folder of the application.") });
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(removePackage,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("k")),
                                 clp.isSet(qSL("f"))));
            break;

        case ListInstallationLocations:
            clp.process(a);
            a.runLater(listInstallationLocations);
            break;

        case ShowInstallationLocation:
            clp.addPositionalArgument(qSL("installation-location"), qSL("The id of an installation location."));
            clp.addOption({ qSL("json"), qSL("Output in JSON format instead of YAML.") });
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            a.runLater(std::bind(showInstallationLocation,
                                 clp.positionalArguments().at(1),
                                 clp.isSet(qSL("json"))));
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
                    throw Exception(Error::IO, "failed to get the current run-state from application-manager: %1").arg(stateReply.error().message());

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
                    throw Exception(Error::IO, "failed to get exit code from application-manager: %1").arg(getReply.error().message());
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
            // on Ctrl+C or SIGTERM -> stop the application
            UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler,
            { SIGTERM, SIGINT
#if defined(Q_OS_UNIX)
                             , SIGPIPE, SIGHUP
#endif
            }, [appId](int /*sig*/) {
                auto reply = dbus.manager()->stopApplication(appId, true);
                reply.waitForFinished();
                qApp->exit(1);
            });

#if defined(POLLRDHUP)
            // ssh does not forward Ctrl+C, but we can detect a hangup condition on stdin
            class HupThread : public QThread // clazy:exclude=missing-qobject-macro
            {
            public:
                HupThread(QCoreApplication *parent)
                    : QThread(parent)
                {
                    connect(parent, &QCoreApplication::aboutToQuit, this, [this]() {
                        if (isRunning()) {
                            terminate();
                            wait();
                        }
                    });
                }

                void run() override
                {
                    while (true) {
                        struct pollfd pfd = { 0, POLLRDHUP, 0 };
                        int res = poll(&pfd, 1, -1);
                        if (res == 1 && pfd.revents & POLLHUP) {
                            kill(getpid(), SIGHUP);
                            return;
                        }
                    }
                }
            };
            (new HupThread(qApp))->start();
#endif // defined(POLLRDHUP)
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

    fprintf(stdout, "%s\n", qPrintable(reply.value().join(qL1C('\n'))));
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

void installPackage(const QString &package, const QString &location, bool acknowledge) Q_DECL_NOEXCEPT_EXPR(false)
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

    fprintf(stdout, "Starting installation of package %s to %s...\n", qPrintable(packageFile), qPrintable(location));

    dbus.connectToManager();
    dbus.connectToInstaller();

    // all the async lambdas below need to share this variable
    static QString installationId;

    // as soon as we have the manifest available: get the app id and acknowledge the installation

    if (acknowledge) {
        QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskRequestingInstallationAcknowledge,
                         [](const QString &taskId, const QVariantMap &metadata) {
            if (taskId != installationId)
                return;
            QString applicationId = metadata.value(qSL("id")).toString();
            if (applicationId.isEmpty())
                throw Exception(Error::IO, "could not find a valid application id in the package");
            fprintf(stdout, "Acknowledging package installation...\n");
            dbus.installer()->acknowledgePackageInstallation(taskId);
        });
    }

    // on failure: quit

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to install package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFinished,
                     [](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Package installation finished successfully.\n");
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus.installer()->startPackageInstallation(location, fi.absoluteFilePath());
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call startPackageInstallation via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "startPackageInstallation returned an empty taskId");
}

void removePackage(const QString &applicationId, bool keepDocuments, bool force) Q_DECL_NOEXCEPT_EXPR(false)
{
    fprintf(stdout, "Starting removal of package %s...\n", qPrintable(applicationId));

    dbus.connectToManager();
    dbus.connectToInstaller();

    // both the async lambdas below need to share this variables
    static QString installationId;

    // on failure: quit

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to remove package: %1 (code: %2)").arg(errorString).arg(errorCode);
    });

    // on success

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFinished,
                     [](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Package removal finished successfully.\n");
        qApp->quit();
    });

    // start the package installation

    auto reply = dbus.installer()->removePackage(applicationId, keepDocuments, force);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call removePackage via DBus: %1").arg(reply.error().message());

    installationId = reply.value();
    if (installationId.isEmpty())
        throw Exception(Error::IO, "removePackage returned an empty taskId");
}

void listInstallationLocations() Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToInstaller();

    auto reply = dbus.installer()->installationLocationIds();
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call installationLocationIds via DBus: %1").arg(reply.error().message());

    fprintf(stdout, "%s\n", qPrintable(reply.value().join(qL1C('\n'))));
    qApp->quit();
}

void showInstallationLocation(const QString &location, bool asJson) Q_DECL_NOEXCEPT_EXPR(false)
{
    dbus.connectToInstaller();

    auto reply = dbus.installer()->getInstallationLocation(location);
    reply.waitForFinished();
    if (reply.isError())
        throw Exception(Error::IO, "failed to call getInstallationLocation via DBus: %1").arg(reply.error().message());

    QVariant app = reply.value();
    fprintf(stdout, "%s\n", asJson ? QJsonDocument::fromVariant(app).toJson().constData()
                                   : QtYaml::yamlFromVariantDocuments({ app }).constData());
    qApp->quit();
}
