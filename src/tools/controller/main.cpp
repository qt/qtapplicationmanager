/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
#include <QDBusError>

#include "global.h"
#include "error.h"
#include "exception.h"
#include "applicationmanager_interface.h"
#include "applicationinstaller_interface.h"
#include "qtyaml.h"
#include "dbus-utilities.h"

AM_USE_NAMESPACE

class DBus : public QObject {
public:
    DBus()
    {
        registerDBusTypes();
    }

    void connectToManager() throw(Exception)
    {
        if (m_manager)
            return;

        auto conn = connectTo(qSL("io.qt.ApplicationManager"));
        m_manager = new IoQtApplicationManagerInterface(qSL("io.qt.ApplicationManager"), qSL("/ApplicationManager"), conn, this);
    }

    void connectToInstaller() throw(Exception)
    {
        if (m_installer)
            return;

        auto conn = connectTo(qSL("io.qt.ApplicationInstaller"));
        m_installer = new IoQtApplicationInstallerInterface(qSL("io.qt.ApplicationManager"), qSL("/ApplicationInstaller"), conn, this);
    }

private:
    QDBusConnection connectTo(const QString &iface) throw(Exception)
    {
        QDBusConnection conn(iface);

        QFile f(QDir::temp().absoluteFilePath(QString(qSL("%1.dbus")).arg(iface)));
        QString dbus;
        if (f.open(QFile::ReadOnly)) {
            dbus = QString::fromUtf8(f.readAll());
            if (dbus == qL1S("system"))
                conn = QDBusConnection::systemBus();
            else if (dbus.isEmpty())
                conn = QDBusConnection::sessionBus();
            else
                conn = QDBusConnection::connectToBus(dbus, qSL("custom"));
        }

        if (!conn.isConnected()) {
            throw Exception(Error::IO, "Could not connect to the D-Bus %1 for %2: (%3)")
                .arg(dbus, iface, conn.lastError().message());
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
    InstallPackage,
    StartApplication,
    DebugApplication,
    StopApplication,
    ListApplications,
    ShowApplication
};

static struct {
    Command command;
    const char *name;
    const char *description;
} commandTable[] = {
    { InstallPackage,   "install-package",   "Install a package." },
    { StartApplication, "start-application", "Start an application." },
    { DebugApplication, "debug-application", "Debug an application." },
    { StopApplication,  "stop-application",  "Stop an application." },
    { ListApplications, "list-applications", "List all installed applications." },
    { ShowApplication,  "show-application",  "Show application meta-data." }
};

static Command command(QCommandLineParser &clp)
{
    if (!clp.positionalArguments().isEmpty()) {
        QByteArray cmd = clp.positionalArguments().at(0).toLatin1();

        for (uint i = 0; i < sizeof(commandTable) / sizeof(commandTable[0]); ++i) {
            if (cmd == commandTable[i].name) {
                clp.clearPositionalArguments();
                clp.addPositionalArgument(cmd, commandTable[i].description, cmd);
                return commandTable[i].command;
            }
        }
    }
    return NoCommand;
}

static void installPackage(const QString &package) throw(Exception);
static void startApplication(const QString &appId, const QString &documentUrl);
static void debugApplication(const QString &debugWrapper, const QString &appId, const QMap<QString, int> &stdRedirections, const QString &documentUrl);
static void stopApplication(const QString &appId);
static void listApplications();
static void showApplication(const QString &appId);

class ThrowingApplication : public QCoreApplication
{
public:
    ThrowingApplication(int &argc, char **argv)
        : QCoreApplication(argc, argv)
    { }

    Exception *exception() const
    {
        return m_exception;
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
                     QString(longestName - qstrlen(commandTable[i].name), qL1C(' ')),
                     qL1S(commandTable[i].description));
    }

    desc += qSL("\nMore information about each command can be obtained by running\n  application-controller <command> --help");

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

    try {
        switch (command(clp)) {
        default:
        case NoCommand:
            if (clp.isSet(qSL("version"))) {
#if QT_VERSION < QT_VERSION_CHECK(5,4,0)
                fprintf(stdout, "%s %s\n", qPrintable(QCoreApplication::applicationName()), qPrintable(QCoreApplication::applicationVersion()));
                exit(0);
#else
                clp.showVersion();
#endif
            }
            if (clp.isSet(qSL("help")))
                clp.showHelp();
            clp.showHelp(1);
            break;

        case InstallPackage:
            clp.addPositionalArgument(qSL("package"), qSL("The file name of the package; can be - for stdin."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            installPackage(clp.positionalArguments().at(1));
            break;

        case StartApplication: {
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.addPositionalArgument(qSL("document-url"),   qSL("The optional document-url."), qSL("[document-url]"));
            clp.process(a);

            int args = clp.positionalArguments().size();
            if (args < 2 || args > 3)
                clp.showHelp(1);

            startApplication(clp.positionalArguments().at(1), args == 3 ? clp.positionalArguments().at(2) : QString());
            break;
        }
        case DebugApplication: {
            clp.addOption({ { qSL("i"), qSL("attach-stdin") }, qSL("Attach the app's stdin to the controller's stdin") });
            clp.addOption({ { qSL("o"), qSL("attach-stdout") }, qSL("Attach the app's stdout to the controller's stdout") });
            clp.addOption({ { qSL("e"), qSL("attach-stderr") }, qSL("Attach the app's stderr to the controller's stderr") });
            clp.addPositionalArgument(qSL("debug-wrapper"),  qSL("The name of a configured debug-wrapper."));
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.addPositionalArgument(qSL("document-url"),   qSL("The optional document-url."), qSL("[document-url]"));
            clp.process(a);

            int args = clp.positionalArguments().size();
            if (args < 3 || args > 4)
                clp.showHelp(1);

            QMap<QString, int> stdRedirections;
            if (clp.isSet(qSL("attach-stdin")))
                stdRedirections["in"] = 0;
            if (clp.isSet(qSL("attach-stdout")))
                stdRedirections["out"] = 1;
            if (clp.isSet(qSL("attach-stderr")))
                stdRedirections["err"] = 2;

            debugApplication(clp.positionalArguments().at(1), clp.positionalArguments().at(2),
                             stdRedirections, args == 3 ? clp.positionalArguments().at(2) : QString());
            break;
        }
        case StopApplication:
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            stopApplication(clp.positionalArguments().at(1));
            break;

        case ListApplications:
            clp.process(a);
            listApplications();
            break;

        case ShowApplication:
            clp.addPositionalArgument(qSL("application-id"), qSL("The id of an installed application."));
            clp.process(a);

            if (clp.positionalArguments().size() != 2)
                clp.showHelp(1);

            showApplication(clp.positionalArguments().at(1));
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

void startApplication(const QString &appId, const QString &documentUrl = QString())
{
    dbus.connectToManager();

    QTimer::singleShot(0, [appId, documentUrl]() {
        auto reply = dbus.manager()->startApplication(appId, documentUrl);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call startApplication via DBus: %1").arg(reply.error().message());

        bool ok = reply.value();
        qApp->exit(ok ? 0 : 2);
    });
}

void debugApplication(const QString &debugWrapper, const QString &appId, const QMap<QString, int> &stdRedirections, const QString &documentUrl = QString())
{
    dbus.connectToManager();

    QTimer::singleShot(0, [debugWrapper, appId, stdRedirections, documentUrl]() {
        QDBusPendingReply<bool> reply;
        if (stdRedirections.isEmpty()) {
            reply = dbus.manager()->debugApplication(appId, debugWrapper, documentUrl);
        } else {
            UnixFdMap fdMap;
            for (auto it = stdRedirections.cbegin(); it != stdRedirections.cend(); ++it)
                fdMap.insert(it.key(), QDBusUnixFileDescriptor(it.value()));

            reply = dbus.manager()->debugApplication(appId, debugWrapper, fdMap, documentUrl);
        }

        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call debugApplication via DBus: %1").arg(reply.error().message());

        bool ok = reply.value();
        if (stdRedirections.isEmpty()) {
            qApp->exit(ok ? 0 : 2);
        } else {
            if (!ok)
                qApp->exit(2);
        }
    });
}

void stopApplication(const QString &appId)
{
    dbus.connectToManager();

    QTimer::singleShot(0, [appId]() {
        auto reply = dbus.manager()->stopApplication(appId);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call stopApplication via DBus: %1").arg(reply.error().message());
        qApp->quit();
    });

}

void listApplications()
{
    dbus.connectToManager();

    QTimer::singleShot(0, []() {
        auto reply = dbus.manager()->applicationIds();
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call applicationIds via DBus: %1").arg(reply.error().message());

        fprintf(stdout, "%s\n", qPrintable(reply.value().join(qL1C('\n'))));
        qApp->quit();
    });
}

void showApplication(const QString &appId)
{
    dbus.connectToManager();

    QTimer::singleShot(0, [appId]() {
        auto reply = dbus.manager()->get(appId);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to get application via DBus: %1").arg(reply.error().message());

        QVariant app = reply.value();
        fprintf(stdout, "%s\n", QtYaml::yamlFromVariantDocuments({ app }).constData());
        qApp->quit();
    });
}

void installPackage(const QString &package) throw(Exception)
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

    QFileInfo fi(package);
    if (!fi.exists() || !fi.isReadable() || !fi.isFile())
        throw Exception(Error::IO, "Package file is not readable: %1").arg(packageFile);

    fprintf(stdout, "Starting installation of package %s...\n", qPrintable(packageFile));

    dbus.connectToManager();
    dbus.connectToInstaller();

    // all the async snippets below need to share these variables
    static QString installationId;
    static QString applicationId;

    // start the package installation

    QTimer::singleShot(0, [packageFile]() {
        auto reply = dbus.installer()->startPackageInstallation(qSL("internal-0"), packageFile);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call startPackageInstallation via DBus: %1").arg(reply.error().message());

        installationId = reply.value();
        if (installationId.isEmpty())
            throw Exception(Error::IO, "startPackageInstallation returned an empty taskId");
    });

    // as soon as we have the manifest available: get the app id and acknowledge the installation

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskRequestingInstallationAcknowledge,
                     [](const QString &taskId, const QVariantMap &metadata) {
        if (taskId != installationId)
            return;
        applicationId = metadata.value(qSL("id")).toString();
        if (applicationId.isEmpty())
            throw Exception(Error::IO, "could not find a valid application id in the package - got: %1").arg(applicationId);
        fprintf(stdout, "Acknowledging package installation...\n");
        dbus.installer()->acknowledgePackageInstallation(taskId);
    });

    // on failure: quit

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFailed,
                     [](const QString &taskId, int errorCode, const QString &errorString) {
        if (taskId != installationId)
            return;
        throw Exception(Error::IO, "failed to install package: %1 (code: %2)").arg(errorString, errorCode);
    });

    // when the installation finished successfully: launch the application

    QObject::connect(dbus.installer(), &IoQtApplicationInstallerInterface::taskFinished,
                     [](const QString &taskId) {
        if (taskId != installationId)
            return;
        fprintf(stdout, "Installation finished - launching application...\n");
        auto reply = dbus.manager()->startApplication(applicationId);
        reply.waitForFinished();
        if (reply.isError())
            throw Exception(Error::IO, "failed to call startApplication via DBus: %1").arg(reply.error().message());
        if (!reply.value())
            throw Exception(Error::IO, "startApplication failed");
        qApp->quit();
    });
}
