// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:execute-external-code

#include <iostream>

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
#include "systemd.h"
#include "utilities.h"

using namespace Qt::StringLiterals;
QT_USE_NAMESPACE_AM

Q_LOGGING_CATEGORY(lcBwrap, "am.container.bubblewrap");

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
    if (m_configuration.value(u"bindMountQtPaths"_s).toBool()) {
        for (auto p : { QLibraryInfo::LibrariesPath, QLibraryInfo::LibraryExecutablesPath,
                       QLibraryInfo::BinariesPath, QLibraryInfo::PluginsPath,
                       QLibraryInfo::Qml2ImportsPath, QLibraryInfo::ArchDataPath,
                       QLibraryInfo::DataPath, QLibraryInfo::TranslationsPath,
                       QLibraryInfo::SettingsPath}) {
            const auto lip = QLibraryInfo::path(p);
            if (!lip.isEmpty() && QDir(lip).exists())
                m_bwrapArguments += { u"--ro-bind"_s, lip, lip };
        }
    }

    const QStringList envFiles = variantToStringList(m_configuration.value(u"environmentFiles"_s));
    for (const auto &envFile : envFiles) {
        QFile f(envFile);
        if (f.size() > 1024*1024) { // 1MiB
            qCWarning(lcBwrap) << "Environment file" << envFile << "is too large (> 1MiB), ignoring.";
            continue;
        }
        if (!f.open(QIODevice::ReadOnly)) {
            qCCritical(lcBwrap) << "Couldn't open environmentFile" << envFile << ":" << f.errorString();
            continue;
        }
        const auto envMap = Systemd::parseEnvironmentFile(QString::fromLocal8Bit(f.readAll()));
        for (const auto &[key, value] : envMap.asKeyValueRange())
            m_bwrapArguments += { u"--setenv"_s, key, value };
    }

    QVariant config = m_configuration.value(u"configuration"_s);
    if (config.metaType() == QMetaType::fromType<QVariantMap>()) {
        qCWarning(lcBwrap) << "Using an unordered map for the bwrap configuration is deprecated. Please convert to a list of key-value pairs.";
        QVariantList to;
        QVariantMap from = config.toMap();
        for (auto it = from.cbegin(); it != from.cend(); ++it)
            to.append(QVariantMap { { it.key(), it.value() } });
        config = to;
    }
    const QVariantList configList = config.toList();
    for (const QVariant &entry : configList) {
        if (entry.metaType() == QMetaType::fromType<QString>()) {
            m_bwrapArguments += u"--"_s + entry.toString();
        } else if (entry.metaType() == QMetaType(QMetaType::QVariantMap)) {
            QVariantMap entryMap = entry.toMap();

            if (entryMap.size() != 1) {
                qCWarning(lcBwrap) << "The bwrap configuration list entries need to be single key-value pairs.";
                continue;
            }

            const QString entryKey = entryMap.firstKey();
            const QVariant entryValue = entryMap.first();

            if (entryValue.metaType() == QMetaType(QMetaType::QVariantMap)) {
                const QVariantMap valueMap = entryValue.toMap();
                for (auto vit = valueMap.constBegin(); vit != valueMap.constEnd(); ++vit ) {
                    const auto valueList = variantToStringList(vit.value());
                    for (const QString &value : valueList) {
                        m_bwrapArguments += u"--"_s + entryKey;
                        m_bwrapArguments += vit.key();
                        m_bwrapArguments += value;
                    }
                }
            } else if (entryValue.metaType() == QMetaType(QMetaType::QVariantList)) {
                QVariantList valueList = entryValue.toList();
                for (auto vit = valueList.constBegin(); vit != valueList.constEnd(); ++vit ) {
                    const auto valueList = variantToStringList(*vit);
                    for (const QString &value : valueList) {
                        m_bwrapArguments += u"--"_s + entryKey;
                        m_bwrapArguments += value;
                    }
                }
            } else if (entryValue.canConvert<QString>()) {
                m_bwrapArguments += u"--"_s + entryKey;
                m_bwrapArguments += entryValue.toString();
            }
        } else {
            qCWarning(lcBwrap) << "The bwrap configuration needs to be a list of strings and/or single key-value pairs.";
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


bool BubblewrapContainer::s_hasCGroupV2 = false;

BubblewrapContainer::BubblewrapContainer(BubblewrapContainerManager *manager, const QVector<int> &stdioRedirections, const QMap<QString, QString> &debugWrapperEnvironment,
                                         const QStringList &debugWrapperCommand)
    : m_manager(manager)
    , m_containerPath(u"/app"_s)
    , m_stdioRedirections(stdioRedirections)
    , m_debugWrapperEnvironment(debugWrapperEnvironment)
    , m_debugWrapperCommand(debugWrapperCommand)
{
    s_hasCGroupV2 = QFile::exists(u"/sys/fs/cgroup/cgroup.controllers"_s);
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

    Q_ASSERT(!m_application.value(u"id"_s).toString().isEmpty());
    setupCustomBindMounts();

    if (m_state == Running && m_namespacePid != 0) {
        // attaching to an existing quick-launcher instance

        for (const auto &[hostPath, containerPath] : m_roBindMounts.asKeyValueRange()) {
            try {
                qCDebug(lcBwrap) << "Mounting app specific mount path from" << hostPath << "to"<< containerPath;
                m_manager->helpers()->bindMountFileSystem(hostPath, containerPath, true, m_namespacePid);
            } catch (const std::exception &e) {
                qCWarning(lcBwrap) << "Mounting the app specific mount path from" << hostPath
                                   << "to" << containerPath << "failed:" << e.what();
                return false;
            }
        }

        for (const auto &[hostPath, containerPath] : m_rwBindMounts.asKeyValueRange()) {
            try {
                qCDebug(lcBwrap) << "Mounting app specific mount path from" << hostPath << "to" << containerPath;
                m_manager->helpers()->bindMountFileSystem(hostPath, containerPath, false, m_namespacePid);
            } catch (const std::exception &e) {
                qCWarning(lcBwrap) << "Mounting the app specific mount path from" << hostPath
                                   << "to" << containerPath << "failed:" << e.what();
                return false;
            }
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

void BubblewrapContainer::setupCustomBindMounts(bool ignoreCapabilities)
{
    m_roBindMounts.clear();
    m_rwBindMounts.clear();

    auto customBindMounts = m_manager->configuration().value(u"customBindMounts"_s).toMap();
    if (customBindMounts.contains(u"app"_s))
        m_containerPath = customBindMounts.value(u"app"_s).toString();
    m_roBindMounts.insert(m_hostPath, m_containerPath);

    QString appId = m_application.value(u"id"_s).toString();
    QStringList capabilities = m_application.value(u"capabilities"_s).toStringList();

    auto addToMountList = [appId, this](const QString &hostPath, const QString &containerPath, bool readOnly) {
        // validateIdForFilesystemUsage() should prevent this, but just to be on the safe side
        if (appId.contains(u'/') || appId.contains(u".."_s)) {
            qCCritical(lcBwrap) << "Invalid application id for filesystem usage:" << appId;
            return;
        }

        QString newPath = hostPath;
        newPath = newPath.replace(u"%APPLICATION_ID%"_s, appId);

        if (containerPath.contains(u"%APPLICATION_ID%"_s)) {
            qCCritical(lcBwrap) << "Can't substitute %APPLICATION_ID% for mount destination paths:" << containerPath;
            return;
        }

        if (readOnly)
            m_roBindMounts.insert(newPath, containerPath);
        else
            m_rwBindMounts.insert(newPath, containerPath);
    };

    auto extra = customBindMounts.value(u"extra"_s).toMap();
    for (const auto &[key, value] : extra.asKeyValueRange()) {
        if (value.canConvert<QVariantMap>()) {
            QVariantMap config = value.toMap();
            if (!config.contains(u"path"_s)) {
                qCCritical(lcBwrap) << "Invalid customBindMounts/extra config: No path configured for" << key;
                continue;
            }
            const QString devicePath = config.value(u"path"_s).toString();
            const QString mode = config.value(u"mode"_s, u"rw"_s).toString();
            bool readOnly = false;
            if (mode == u"ro") {
                readOnly = true;
            } else if (mode == u"rw") {
                readOnly = false;
            } else {
                qCCritical(lcBwrap) << "Invalid customMount config: Invalid option" << mode << "for 'mode' (expected 'ro' or 'rw')";
                continue;
            }

            if (!ignoreCapabilities) {
                bool capabilitiesOk = true;
                const QStringList neededCapabilities = config.value(u"capabilities"_s).toStringList();
                for (const auto &cap : neededCapabilities) {
                    if (!capabilities.contains(cap)) {
                        capabilitiesOk = false;
                        break;
                    }
                }
                if (!capabilitiesOk) {
                    qCDebug(lcBwrap) << "Ignoring customMount config" << key << "because app" << appId << "does not have all the required capabilities";
                    continue;
                }
            }
            addToMountList(key, devicePath, readOnly);
        } else {
            addToMountList(key, value.toString(), false /*readOnly*/);
        }
    }
}

QString BubblewrapContainer::controlGroup() const
{
    return m_currentControlGroup;
}

bool BubblewrapContainer::setControlGroup(const QString &groupName)
{
    if (!s_hasCGroupV2)
        return false;

    if (groupName == m_currentControlGroup)
        return true;

    // cgroup-v2 nested groups use '/', but '..' or a leading '/' would escape /sys/fs/cgroup/
    if (groupName.startsWith(u'/') || groupName.split(u'/').contains(u".."_s)) {
        qCWarning(lcBwrap) << "Refusing to set cgroup with invalid name:" << groupName;
        return false;
    }

    //qCWarning(lcBwrap) << "Setting cgroup for" << m_program << ", pid" << m_process->processId() << ":" << "->" << groupName;

    QString file = u"/sys/fs/cgroup/%1/cgroup.procs"_s.arg(groupName);
    QFile f(file);

    // A bit awkward, but cgroupfs accepts only one pid per write
    for (quint64 pid : { quint64(m_process->processId()), m_namespacePid }) {
        bool ok = f.open(QFile::WriteOnly);
        QByteArray pidString = QByteArray::number(pid);
        pidString.append('\n');
        ok = ok && (f.write(pidString) == pidString.size());

        if (!ok) {
            qCWarning(lcBwrap) << "Failed setting cgroup for" << m_program << ", pid"
                               << pid << "to" << groupName;
            return false;
        }
        f.close();
    }

    m_currentControlGroup = groupName;
    return true;
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
    // Check if the host path matches any of the configured bind mounts
    // Use the longest matching path to handle nested mounts correctly
    QString bestMatch;
    QString bestContainerPath;

    auto checkMount = [&](const QMap<QString, QString> &mounts) {
        for (const auto &[hostMountPath, containerMountPath] : mounts.asKeyValueRange()) {
            if (hostMountPath.length() > bestMatch.length()
                    && hostPath.startsWith(hostMountPath)) {
                bestMatch = hostMountPath;
                bestContainerPath = containerMountPath;
            }
        }
    };
    checkMount(m_roBindMounts);
    checkMount(m_rwBindMounts);

    if (!bestMatch.isEmpty())
        return QDir::cleanPath(bestContainerPath + u'/' + hostPath.mid(bestMatch.length()));

    // If no mapping found, return the host path as-is
    return hostPath;
}

bool BubblewrapContainer::start(const QStringList &arguments, const QMap<QString, QString> &runtimeEnvironment,
                                const QVariantMap &amConfig)
{
    if (!QFile::exists(m_program))
        return false;

    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_ASSERT(sizeof(QProcess::ProcessError) == sizeof(ContainerInterface::ProcessError));
        auto processError = static_cast<ContainerInterface::ProcessError>(error);

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
    if (::pipe2(m_statusPipeFd.data(), O_NONBLOCK) == -1) {
        qCWarning(lcBwrap) << "Couldn't create the status pipe:" << qt_error_string(errno);
        return false;
    }

    bool stopBeforeExec = m_manager->configuration().value(u"stopBeforeExec"_s).toBool();

    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);
    m_process->setChildProcessModifier([this, stopBeforeExec]() {
          // copied from processcontainer, this could be moved into a helper
        if (stopBeforeExec) {
            std::cerr << "\n*** a 'process' container was started in stopped state ***\n"
                         "The process is suspended via SIGSTOP and you can attach a debugger to it via\n"
                         "\n   gdb -p " << ::getpid() << "\n\n";
            ::raise(SIGSTOP);
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
    auto *sn = new QSocketNotifier(m_statusPipeFd[0], QSocketNotifier::Read, this);
    connect(sn, &QSocketNotifier::activated, this, [this, sn](int pipeFd) {
        do {
            std::array<char, 1024> buffer;
            qsizetype bytesRead = qt_safe_read(pipeFd, buffer.data(), buffer.size());

            if (bytesRead <= 0) {
                // eof or hard error
                if ((bytesRead == 0) || (errno != EAGAIN)) {
                    qt_safe_close(pipeFd);
                    sn->setEnabled(false);
                }
                break;
            }
            m_statusBuffer.append(buffer.data(), bytesRead);
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
                    QMetaObject::invokeMethod(this, [this] { stop(ForcedExit); }, Qt::QueuedConnection);
                }

            }
            auto exitCodeIt = root.constFind(u"exit-code"_s);
            if (exitCodeIt != root.constEnd()) {
                m_hasExitCode = true;
                int exitCode = int(exitCodeIt->toInteger());

                // bwrap uses bash conventions to communicate a signal kill: 128 + signal number
                if ((exitCode > 128) && (exitCode < 193)) {
                    m_exitCode = exitCode - 128;
                    m_exitStatus = QProcess::CrashExit;
                } else {
                    m_exitCode = exitCode;
                    m_exitStatus = QProcess::NormalExit;
                }
            }
        } while (true);
    });

    // Calculate the exact brwap command to run
    QStringList bwrapCommand = m_manager->bwrapArguments();

    // Pass the write end of the pipe to bwrap
    bwrapCommand += { u"--json-status-fd"_s, QString::number(m_statusPipeFd[1]) };

    try {
        // export all additional sockets
        auto *h = manager()->helpers();

        const QString dbusP2PSocket = h->checkDBusSocketPath(
            amConfig.value(u"dbus"_s).toMap().value(u"p2p"_s).toString(), "P2P");
        bwrapCommand += { u"--ro-bind"_s, dbusP2PSocket, dbusP2PSocket };

        const QString dbusSessionBusAddress = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        if (!dbusSessionBusAddress.isEmpty()) {
            const QString sessionBusSocket = h->checkDBusSocketPath(dbusSessionBusAddress, "Session");
            bwrapCommand += { u"--ro-bind"_s, sessionBusSocket, sessionBusSocket };
            bwrapCommand += { u"--setenv"_s, u"DBUS_SESSION_BUS_ADDRESS"_s, dbusSessionBusAddress };
        }

        const QString xdgRuntimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
        const QString waylandDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
        const QString waylandSocket = h->checkWaylandSocketPath(xdgRuntimeDir, waylandDisplay);

        bwrapCommand += { u"--ro-bind"_s, waylandSocket, waylandSocket };
        bwrapCommand += { u"--setenv"_s, u"XDG_RUNTIME_DIR"_s, xdgRuntimeDir };
        bwrapCommand += { u"--setenv"_s, u"WAYLAND_DISPLAY"_s, waylandDisplay };

    } catch (const std::exception &e) {
        qCWarning(lcBwrap) << e.what();
        return false;
    }

    // If the m_roBindMounts are already available we can mount them directly
    // Otherwise we are quick launching a container and have to make sure the container path exists
    // to be able to mount to it afterwards.
    if (!m_roBindMounts.isEmpty() || !m_rwBindMounts.isEmpty()) {
        for (const auto &[hostPath, containerPath] : m_roBindMounts.asKeyValueRange())
            bwrapCommand += { u"--ro-bind"_s, hostPath, containerPath };
        for (const auto &[hostPath, containerPath] : m_rwBindMounts.asKeyValueRange())
            bwrapCommand += { u"--bind"_s, hostPath, containerPath };
    } else {
        // We can't check for the capabilities as we don't know the app yet, but we already need
        // to know the paths in order be able to mount them later.
        setupCustomBindMounts(true /*ignoreCapabilities*/);
        for (const auto & [hostPath, containerPath] : m_roBindMounts.asKeyValueRange())
            bwrapCommand += { u"--dir"_s, containerPath };
        for (const auto & [hostPath, containerPath] : m_rwBindMounts.asKeyValueRange())
            bwrapCommand += { u"--dir"_s, containerPath };
    }

    // Add all needed env variables
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

void BubblewrapContainer::stop(ExitStatus exitStatus)
{
    if (!m_process)
        return;

    switch (exitStatus) {
    case NormalExit:
        m_process->terminate();
        break;
    case ForcedExit:
        m_process->kill();
        break;
    case CrashExit:
        if (auto pid = m_process->processId())
            ::kill((pid_t) pid, SIGSEGV);
        break;
    case WatchdogExit:
        if (auto pid = m_process->processId()) {
            if (int sig = manager()->helpers()->watchdogSignal())
                ::kill((pid_t) pid, sig);
        }
        break;
    }
}

BubblewrapContainer::RunState BubblewrapContainer::state() const
{
    return m_state;
}

void BubblewrapContainer::containerExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_hasExitCode) { // bwrap may have crashed, but the app itself may have exited differently
        exitCode = m_exitCode;
        exitStatus = m_exitStatus;
    }

    ExitStatus status = NormalExit;

    if (exitStatus == QProcess::CrashExit) {
        if ((exitCode == SIGTERM || exitCode == SIGKILL))
            status = ForcedExit;
        else if (exitCode == manager()->helpers()->watchdogSignal())
            status = WatchdogExit;
        else
            status = CrashExit;
    }

    m_state = NotRunning;
    emit stateChanged(m_state);
    emit finished(exitCode, status);

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
