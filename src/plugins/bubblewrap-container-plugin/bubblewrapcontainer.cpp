// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <tuple>

#include <QJsonDocument>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QDir>
#include <QtCore/private/qcore_unix_p.h> // qt_safe_read

#include <unistd.h>
#include <sys/stat.h>
#include "bubblewrapcontainer.h"

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcBwrap, "am.container.bubblewrap");

// This could be moved into a helper class
// copy from utilities.h
inline QStringList variantToStringList(const QVariant &v)
{
    return (v.metaType() == QMetaType::fromType<QString>()) ? QStringList(v.toString())
                                                            : v.toStringList();
}

BubblewrapContainerManager::BubblewrapContainerManager()
{
    static bool once = false;
    if (!once) {
        once = true;
    }
}

bool BubblewrapContainerManager::initialize(ContainerHelperFunctions *helpers)
{
    m_helpers = helpers;

    if (!QFile::exists(m_bwrapPath)) {
        qCWarning(lcBwrap) << "Couldn't find the bwrap executable";
        return false;
    }

    return true;
}

QString BubblewrapContainerManager::identifier() const
{
    return u"bubblewrap"_s;
}

bool BubblewrapContainerManager::supportsQuickLaunch() const
{
    if (!m_helpers->hasRootPrivileges())
        qCWarning(lcBwrap) << "bubblewrap needs root privileges to support quick-launch";

    return m_helpers->hasRootPrivileges();
}

void BubblewrapContainerManager::setConfiguration(const QVariantMap &configuration)
{
    m_configuration = configuration;

    m_bwrapPath = m_configuration.value(u"bwrap-location"_s).toString();
    if (m_bwrapPath.isEmpty())
        m_bwrapPath =  QStandardPaths::findExecutable(u"bwrap"_s);

    // for development only - mount the user's $HOME dir into the container as read-only. Otherwise
    // you would have to `make install` the AM into /usr on every rebuild
    if (m_configuration.value(u"bindMountHome"_s).toBool())
        m_bwrapArguments += { u"--ro-bind"_s, QDir::homePath(), QDir::homePath() };

    m_bwrapArguments += u"--clearenv"_s;

    // network setup
    QStringList sharedNamespaces = { u"-all"_s };

    const QVariant unshareNetwork = m_configuration.value(u"unshareNetwork"_s);
    if (unshareNetwork.isValid()) {
        qCWarning(lcBwrap) << "The 'unshareNetwork' config value is deprecated, use 'sharedNamespaces' instead";

        if (unshareNetwork.typeId() == QMetaType::Bool) {
            if (unshareNetwork.toBool() == false)
                sharedNamespaces.append(u"+net"_s);
        } else {
            m_networkSetupScript = unshareNetwork.toString();
        }
    }
    static const QStringList knownNamespaces = { u"all"_s, u"net"_s, u"user"_s, u"ipc"_s,
                                                u"pid"_s, u"net"_s, u"uts"_s, u"cgroup"_s };

    sharedNamespaces = m_configuration.value(u"sharedNamespaces"_s, sharedNamespaces).toStringList();

    QStringList namespaceList;
    bool minusAll = true;
    bool firstNSEntry = true;
    for (const auto &sns : std::as_const(sharedNamespaces)) {
        bool plus = sns.startsWith(u'+');
        bool minus = sns.startsWith(u'-');

        if (!plus && !minus) {
            qCWarning(lcBwrap) << "'sharedNamespaces' must start with + or -, ignoring" << sns;
            continue;
        }

        const QString ns = sns.mid(1);
        if (!knownNamespaces.contains(ns)) {
            qCWarning(lcBwrap) << "'sharedNamespaces' can only be one of" << knownNamespaces.join(u", ")
                               << ", ignoring" << sns;
            continue;
        }

        if (firstNSEntry) {
            if (ns != u"all"_s) {
                qCWarning(lcBwrap) << "'sharedNamespaces' must start with +all or -all, ignoring" << sns;
                break;
            }
            minusAll = minus;
            firstNSEntry = false;
        } else {
            if ((plus && !minusAll) || (minus && minusAll)) {
                qCWarning(lcBwrap) << "'sharedNamespaces' should not repeat the +/- from the first 'all' entry, ignoring" << sns;
                continue;
            }
            namespaceList << ns;
        }
    }
    bool sharedNetwork = true; // for better diagnostics down below

    if (minusAll) { // unshare everything, but...
        if (namespaceList.isEmpty()) {
            m_bwrapArguments += u"--unshare-all"_s;
            sharedNetwork = false;
        } else {
            const auto allNamespaces = knownNamespaces.mid(1); // skip "all"
            for (const auto &ns : allNamespaces) {
                if (!namespaceList.contains(ns)) {
                    m_bwrapArguments += u"--unshare-"_s + ns;
                    sharedNetwork = sharedNetwork && (ns != u"net"_s);
                }
            }
        }
    } else { // share everything, but...
        for (const auto &ns : std::as_const(namespaceList)) {
            m_bwrapArguments += u"--unshare-"_s + ns;
            sharedNetwork = sharedNetwork && (ns != u"net"_s);
        }
    }

    m_networkSetupScript = m_configuration.value(u"networkSetupScript"_s, m_networkSetupScript).toString();

    if (!m_networkSetupScript.isEmpty() && sharedNetwork)
        qCWarning(lcBwrap) << "'networkSetupScript' is set, but the network namespace is already shared via 'sharedNamespaces'.";

    m_bwrapArguments += u"--die-with-parent"_s;
    m_bwrapArguments += u"--new-session"_s;

    // export all needed Qt paths
    for (auto p : { QLibraryInfo::LibrariesPath, QLibraryInfo::LibraryExecutablesPath,
                   QLibraryInfo::BinariesPath, QLibraryInfo::PluginsPath,
                   QLibraryInfo::Qml2ImportsPath, QLibraryInfo::ArchDataPath,
                   QLibraryInfo::DataPath, QLibraryInfo::TranslationsPath,
                   QLibraryInfo::SettingsPath}) {
        const auto lip = QLibraryInfo::path(p);
        if (!lip.isEmpty() && QDir(lip).exists())
            m_bwrapArguments += { u"--ro-bind"_s, lip, lip };
    }

    const QVariantMap config = m_configuration.value(u"configuration"_s).toMap();
    for (auto it = config.constBegin(); it != config.constEnd(); ++it ) {
        if (it.value().isNull()) {
            m_bwrapArguments += u"--"_s + it.key();
            continue;
        }

        if (it.value().metaType() == QMetaType(QMetaType::QVariantMap)) {
            QVariantMap valueMap = it.value().toMap();
            for (auto vit = valueMap.constBegin(); vit != valueMap.constEnd(); ++vit ) {
                const auto valueList = variantToStringList(vit.value());
                for (const QString &value : valueList) {
                    m_bwrapArguments += u"--"_s + it.key();
                    m_bwrapArguments += vit.key();
                    m_bwrapArguments += value;
                }
            }
            continue;
        }

        if (it.value().metaType() == QMetaType(QMetaType::QVariantList)) {
            QVariantList valueList = it.value().toList();
            for (auto vit = valueList.constBegin(); vit != valueList.constEnd(); ++vit ) {
                const auto valueList = variantToStringList(*vit);
                for (const QString &value : valueList) {
                    m_bwrapArguments += u"--"_s + it.key();
                    m_bwrapArguments += value;
                }
            }
            continue;
        }

        if (it.value().canConvert<QString>()) {
            m_bwrapArguments += u"--"_s + it.key();
            m_bwrapArguments += it.value().toString();
            continue;
        }
    }
}

ContainerInterface *BubblewrapContainerManager::create(bool isQuickLaunch, const QVector<int> &stdioRedirections,
                                                     const QMap<QString, QString> &debugWrapperEnvironment,
                                                     const QStringList &debugWrapperCommand)
{
    Q_UNUSED(isQuickLaunch)

    return new BubblewrapContainer(this, stdioRedirections, debugWrapperEnvironment, debugWrapperCommand);
}

QString BubblewrapContainerManager::bwrapPath() const
{
    return m_bwrapPath;
}

QVariantMap BubblewrapContainerManager::configuration() const
{
    return m_configuration;
}

ContainerHelperFunctions *BubblewrapContainerManager::helpers() const
{
    return m_helpers;
}

QStringList BubblewrapContainerManager::bwrapArguments() const
{
    return m_bwrapArguments;
}

QString BubblewrapContainerManager::networkSetupScript() const
{
    return m_networkSetupScript;
}



BubblewrapContainer::BubblewrapContainer(BubblewrapContainerManager *manager, const QVector<int> &stdioRedirections, const QMap<QString, QString> &debugWrapperEnvironment,
                                         const QStringList &debugWrapperCommand)
    : m_manager(manager)
    , m_containerPath(u"/app"_s)
    , m_stdioRedirections(stdioRedirections)
    , m_debugWrapperEnvironment(debugWrapperEnvironment)
    , m_debugWrapperCommand(debugWrapperCommand)
{
}

BubblewrapContainer::~BubblewrapContainer()
{
    if (m_statusPipeFd[0] >= 0)
        QT_CLOSE(m_statusPipeFd[0]);
    if (m_statusPipeFd[1] >= 0)
        QT_CLOSE(m_statusPipeFd[1]);

    manager()->helpers()->closeAndClearFileDescriptors(m_stdioRedirections);
}

BubblewrapContainerManager *BubblewrapContainer::manager() const
{
    return m_manager;
}

bool BubblewrapContainer::attachApplication(const QVariantMap &application)
{
    // In normal launch, attachApplication is called first, then the start()
    // method is called. During quicklaunch start() is called first and then
    // attachApplication.

    m_application = application;

    m_hostPath = application.value(u"codeDir"_s).toString();
    if (m_hostPath.isEmpty())
        m_hostPath = QDir::currentPath();

    m_appRelativeCodePath = application.value(u"codeFilePath"_s).toString();

    if (m_state == Running && m_namespacePid != 0) {
        // attaching to an existing quick-launcher instance

        try {
            m_manager->helpers()->bindMountFileSystem(m_hostPath, m_containerPath, true, m_namespacePid);
        } catch (const std::exception &e) {
            qCWarning(lcBwrap) << "Mounting the application directory failed:" << e.what();
            return false;
        }
        if (!runNetworkSetupScript(NetworkScriptEvent::Start)) {
            qCWarning(lcBwrap) << "Network setup (start app in quick-launcher) failed!";
            return false;
        }
    }

    m_ready = true;
    emit ready();
    return true;
}

QString BubblewrapContainer::controlGroup() const
{
    return QString();
}

bool BubblewrapContainer::setControlGroup(const QString &groupName)
{
    Q_UNUSED(groupName)
    return false;
}

bool BubblewrapContainer::setProgram(const QString &program)
{
    m_program = program;
    return true;
}

void BubblewrapContainer::setBaseDirectory(const QString &baseDirectory)
{
    m_baseDir = baseDirectory;
}

bool BubblewrapContainer::isReady() const
{
    return m_ready;
}

QString BubblewrapContainer::mapContainerPathToHost(const QString &containerPath) const
{
    return containerPath;
}

QString BubblewrapContainer::mapHostPathToContainer(const QString &hostPath) const
{
    QString containerPath = m_containerPath;

    QFileInfo fileInfo(hostPath);
    if (fileInfo.isFile())
        containerPath = m_containerPath + u"/"_s + fileInfo.fileName();

    return containerPath;
}

bool BubblewrapContainer::start(const QStringList &arguments, const QMap<QString, QString> &runtimeEnvironment,
                                const QVariantMap &amConfig)
{
    if (!QFile::exists(m_program))
        return false;

    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_ASSERT(sizeof(QProcess::ProcessError) == sizeof(ContainerInterface::ProcessError));
        ContainerInterface::ProcessError processError = static_cast<ContainerInterface::ProcessError>(error);

        emit errorOccured(processError);
    });
    connect(m_process, &QProcess::started, this, [this]() {
        // Close write end of the pipe
        QT_CLOSE(m_statusPipeFd[1]);
        m_statusPipeFd[1] = -1;

        m_pid = m_process->processId();

        emit started();
    });
    connect(m_process, &QProcess::finished, this, &BubblewrapContainer::containerExited);
    connect(m_process, &QProcess::stateChanged, this, [this](QProcess::ProcessState processState) {
        switch (processState) {
            case QProcess::NotRunning: m_state = ContainerInterface::NotRunning; break;
            case QProcess::Starting: m_state = ContainerInterface::StartingUp; break;
            case QProcess::Running: m_state = ContainerInterface::Running; break;
        }
        emit stateChanged(m_state);
    });

    // Calculate the exact app command to run
    QStringList appCmd;
    if (!m_debugWrapperCommand.isEmpty()) {
        appCmd += manager()->helpers()->substituteCommand(m_debugWrapperCommand, m_program, arguments);
    } else {
        appCmd += m_program;
        appCmd += arguments;
    }

    // Create a pipe which is used by bwrap to communicate its status e.g. the used namespaces
    if (::pipe2(m_statusPipeFd, O_NONBLOCK) == -1) {
        qCWarning(lcBwrap) << "Couldn't create the status pipe:" << qt_error_string(errno);
        return false;
    }

    bool stopBeforeExec = m_manager->configuration().value(u"stopBeforeExec"_s).toBool();

    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);
    m_process->setChildProcessModifier([this, stopBeforeExec]() {
          // copied from processcontainer, this could be moved into a helper
        if (stopBeforeExec) {
            fprintf(stderr, "\n*** a 'process' container was started in stopped state ***\nthe process is suspended via SIGSTOP and you can attach a debugger to it via\n\n   gdb -p %d\n\n", getpid());
            raise(SIGSTOP);
        }
        // duplicate any requested redirections to the respective stdin/out/err fd. Also make sure to
        // close the original fd: otherwise we would block the tty where the fds originated from.
        for (int i = 0; i < 3; ++i) {
            int fd = m_stdioRedirections.value(i, -1);
            if (fd >= 0) {
                dup2(fd, i);
                ::close(fd);
            }
        }
        // Close read end of the pipe
        QT_CLOSE(m_statusPipeFd[0]);
    });

    // read from fifo and dump to message handler
    QSocketNotifier *sn = new QSocketNotifier(m_statusPipeFd[0], QSocketNotifier::Read, this);
    connect(sn, &QSocketNotifier::activated, this, [this, sn](int pipeFd) {
        do {
            char buffer[1024];
            qsizetype bytesRead = qt_safe_read(pipeFd, buffer, sizeof(buffer));

            if (bytesRead <= 0) {
                // eof or hard error
                if ((bytesRead == 0) || (errno != EAGAIN)) {
                    qt_safe_close(pipeFd);
                    sn->setEnabled(false);
                }
                break;
            }
            m_statusBuffer.append(buffer, bytesRead);
        } while (true);

        do {
            auto index = m_statusBuffer.indexOf('\n');
            if (index < 0)
                break;
            QByteArray line = m_statusBuffer.left(index + 1);
            m_statusBuffer = m_statusBuffer.mid(index + 1);

            QJsonParseError jsonError;
            QJsonDocument json = QJsonDocument::fromJson(line, &jsonError);
            if (jsonError.error != QJsonParseError::NoError) {
                qCDebug(lcBwrap) << "Parsing bwrap status json failed:" << jsonError.errorString();
                continue;
            }
            auto root = json.object();
            auto childPidIt = root.constFind(u"child-pid"_s);
            if (childPidIt != root.constEnd()) {
                m_namespacePid = quint64(childPidIt->toInteger());
                qCDebug(lcBwrap) << "Namespace pid for app" << m_application.value(u"id"_s).toString()
                                 << "=" << m_namespacePid;

                bool success = false;
                const char *what = nullptr;
                if (m_application.isEmpty()) {
                    // this is a quicklauncher instance
                    success = runNetworkSetupScript(NetworkScriptEvent::QuickLaunch);
                    if (!success)
                        what = "(start quick-launcher)";
                } else {
                    success = runNetworkSetupScript(NetworkScriptEvent::Start);
                    if (!success)
                        what = "(start app)";
                }
                if (!success) {
                    qCWarning(lcBwrap) << "Network setup" << what << "failed!";
                    QMetaObject::invokeMethod(this, &BubblewrapContainer::kill, Qt::QueuedConnection);
                }

            }
            auto exitCodeIt = root.constFind(u"exit-code"_s);
            if (exitCodeIt != root.constEnd()) {
                m_hasExitCode = true;
                m_exitCode = int(exitCodeIt->toInteger());
            }
        } while (true);
    });

    // Calculate the exact brwap command to run
    QStringList bwrapCommand = m_manager->bwrapArguments();

    // Pass the write end of the pipe to bwrap
    bwrapCommand += { u"--json-status-fd"_s, QString::number(m_statusPipeFd[1]) };

    // parse the actual socket file name from the DBus specification
    // This could be moved into a helper class
    QString dbusP2PSocket = amConfig.value(u"dbus"_s).toMap().value(u"p2p"_s).toString();
    dbusP2PSocket = dbusP2PSocket.mid(dbusP2PSocket.indexOf(u'=') + 1);
    dbusP2PSocket = dbusP2PSocket.left(dbusP2PSocket.indexOf(u','));
    QFileInfo dbusP2PInfo(dbusP2PSocket);
    if (!dbusP2PInfo.exists()) {
        qCWarning(lcBwrap) << "p2p dbus socket doesn't exist: " << dbusP2PInfo.absoluteFilePath();
        return false;
    }

    // parse the actual socket file name from the DBus specification
    // This could be moved into a helper class
    QByteArray dbusSessionBusAddress = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    QString sessionBusSocket = QString::fromLocal8Bit(dbusSessionBusAddress);
    sessionBusSocket = sessionBusSocket.mid(sessionBusSocket.indexOf(u'=') + 1);
    sessionBusSocket = sessionBusSocket.left(sessionBusSocket.indexOf(u','));
    QFileInfo sessionBusInfo(sessionBusSocket);
    if (!sessionBusInfo.exists()) {
        qCWarning(lcBwrap) << "session dbus socket doesn't exist: " << sessionBusInfo.absoluteFilePath();
        return false;
    }

    // parse the wayland socket name from wayland env variables
    // This could be moved into a helper class
    QByteArray waylandDisplayName = qgetenv("WAYLAND_DISPLAY");
    QByteArray xdgRuntimeDir = qgetenv("XDG_RUNTIME_DIR");
    QFileInfo waylandDisplayInfo(QString::fromLocal8Bit(xdgRuntimeDir) + u"/"_s + QString::fromLocal8Bit(waylandDisplayName));
    if (!waylandDisplayInfo.exists()) {
        qCWarning(lcBwrap) << "wayland socket doesn't exist: " << waylandDisplayInfo.absoluteFilePath();
        return false;
    }

    // export all additional sockets and the actual appliaction
    bwrapCommand += { u"--ro-bind"_s, dbusP2PInfo.absoluteFilePath(), dbusP2PInfo.absoluteFilePath() };
    bwrapCommand += { u"--ro-bind"_s, sessionBusInfo.absoluteFilePath(), sessionBusInfo.absoluteFilePath() };
    bwrapCommand += { u"--ro-bind"_s, waylandDisplayInfo.absoluteFilePath(), waylandDisplayInfo.absoluteFilePath() };

    // If the hostPath exists we can mount it directly.
    // Otherwise we are quick launching a container and have to make sure the container path exists
    // to be able to mount to it afterwards.
    if (QFile::exists(m_hostPath))
        bwrapCommand += { u"--ro-bind"_s, m_hostPath, m_containerPath };
    else
        bwrapCommand += { u"--dir"_s, m_containerPath };

    // Add all needed env variables
    bwrapCommand += { u"--setenv"_s, u"XDG_RUNTIME_DIR"_s, QString::fromLocal8Bit(xdgRuntimeDir) };
    bwrapCommand += { u"--setenv"_s, u"WAYLAND_DISPLAY"_s, QString::fromLocal8Bit(waylandDisplayName) };
    bwrapCommand += { u"--setenv"_s, u"DBUS_SESSION_BUS_ADDRESS"_s, QString::fromLocal8Bit(dbusSessionBusAddress) };

    const auto allEnvKeys = QProcessEnvironment::systemEnvironment().keys();
    for (const auto &key : allEnvKeys) {
        if (key.startsWith(u"LC_"_s) || key == u"LANG")
            bwrapCommand += { u"--setenv"_s, key, QProcessEnvironment::systemEnvironment().value(key)};
    }

    for (auto it = runtimeEnvironment.constBegin(); it != runtimeEnvironment.constEnd(); ++it) {
        if (it.value().isEmpty())
            bwrapCommand += { u"--unsetenv"_s, it.key() };
        else
            bwrapCommand += { u"--setenv"_s, it.key(), it.value() };
    }

    // set the env variables coming from a debug wrapper
    for (auto it = m_debugWrapperEnvironment.cbegin(); it != m_debugWrapperEnvironment.cend(); ++it) {
        if (it.value().isEmpty())
            bwrapCommand += { u"--unsetenv"_s, it.key() };
        else
            bwrapCommand += { u"--setenv"_s, it.key(), it.value() };
    }

    m_process->setProgram(manager()->bwrapPath());
    QStringList processArguments;
    processArguments += bwrapCommand;
    processArguments += u"--"_s;
    processArguments += appCmd;
    m_process->setArguments(processArguments);
    // Just to be safe start bwrap in a clean environment
    m_process->setProcessEnvironment(QProcessEnvironment());

    // pretty print the bwrap args -- makes it easier to debug
    auto dumpArgs = [](const QStringList &args, const QString &indent) -> QString {
        QString s;
        for (const auto &arg : args) {
            if (arg.startsWith(u'-'))
                s = s + u'\n' + indent + arg;
            else
                s = s + u' ' + arg;
        }
        return s;
    };

    qCDebug(lcBwrap).noquote() << "BubblewrapContainer is trying to launch application"
                               << "\n * directory . " << m_containerPath
                               << "\n * command ... " << m_process->program()
                               << "\n * arguments . " << dumpArgs(m_process->arguments(), u"   "_s);

    m_process->start();

    // we are forked now and the child process has received a copy of all redirected fds
    // now it's time to close our fds, since we don't need them anymore (plus we would block
    // the tty where they originated from)
    manager()->helpers()->closeAndClearFileDescriptors(m_stdioRedirections);

    return true;
}

qint64 BubblewrapContainer::processId() const
{
    return m_pid;
}

BubblewrapContainer::RunState BubblewrapContainer::state() const
{
    return m_state;
}

void BubblewrapContainer::kill()
{
    if (m_process)
        m_process->kill();
}

void BubblewrapContainer::terminate()
{
    if (m_process)
        m_process->terminate();
}

void BubblewrapContainer::containerExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_hasExitCode) { // bwrap may have crashed, but the app terminated normally
        exitCode = m_exitCode;
        exitStatus = QProcess::NormalExit;
    }
    m_state = NotRunning;
    emit stateChanged(m_state);
    emit finished(exitCode, (exitStatus == QProcess::NormalExit) ? ContainerInterface::NormalExit
                                                                 : ContainerInterface::CrashExit);

    if (!runNetworkSetupScript(NetworkScriptEvent::Stop))
        qCWarning(lcBwrap) << "Network setup (stop) failed!";
}

bool BubblewrapContainer::runNetworkSetupScript(NetworkScriptEvent event)
{
    const auto script = manager()->networkSetupScript();
    if (script.isEmpty())
        return true;

    QString eventStr;
    switch (event) {
    case NetworkScriptEvent::Start      : eventStr = u"start"_s; break;
    case NetworkScriptEvent::Stop       : eventStr = u"stop"_s; break;
    case NetworkScriptEvent::QuickLaunch: eventStr = u"quicklaunch"_s; break;
    }

    if (eventStr.isEmpty())
        return false;

    const QString appId = m_application.isEmpty() ? u"quicklaunch"_s
                                                  : m_application.value(u"id"_s).toString();
    QString cmd = script + u" "_s + eventStr + u" \""_s + appId + u"\" "_s
                  + QString::number(m_namespacePid);
    qCDebug(lcBwrap).noquote() << "Running network setup script:" << cmd;

    QProcess p;
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.startCommand(cmd, QIODevice::ReadOnly);
    if (p.waitForStarted() && p.waitForFinished())
        return (p.exitCode() == 0);
    return false;
}


#include "moc_bubblewrapcontainer.cpp"
