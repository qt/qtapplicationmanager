// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <tuple>

#include <QtDBus/QtDBus>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QMetaObject>
#include <qplatformdefs.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "softwarecontainer.h"


QT_BEGIN_NAMESPACE

QDBusArgument &operator<<(QDBusArgument &argument, const QMap<QString,QString> &map)
{
    argument.beginMap(QMetaType::QString, QMetaType::QString);
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        argument.beginMapEntry();
        argument << it.key() << it.value();
        argument.endMapEntry();
    }
    argument.endMap();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QMap<QString,QString> &map)
{
    argument.beginMap();
    while (!argument.atEnd()) {
        argument.beginMapEntry();
        QString key, value;
        argument >> key >> value;
        map.insert(key, value);
        argument.endMapEntry();
    }
    argument.endMap();

    return argument;
}

QT_END_NAMESPACE



// unfortunately, this is a copy of the code from debugwrapper.cpp
static QStringList substituteCommand(const QStringList &debugWrapperCommand, const QString &program,
                                     const QStringList &arguments)
{
    QString stringifiedArguments = arguments.join(QLatin1Char(' '));
    QStringList result;

    for (const QString &s : debugWrapperCommand) {
        if (s == QStringLiteral("%arguments%")) {
            result << arguments;
        } else {
            QString str(s);
            str.replace(QStringLiteral("%program%"), program);
            str.replace(QStringLiteral("%arguments%"), stringifiedArguments);
            result << str;
        }
    }
    return result;
}

SoftwareContainerManager::SoftwareContainerManager()
{
    static bool once = false;
    if (!once) {
        once = true;
        qDBusRegisterMetaType<QMap<QString, QString>>();
    }
}

QString SoftwareContainerManager::identifier() const
{
    return QStringLiteral("softwarecontainer");
}

bool SoftwareContainerManager::supportsQuickLaunch() const
{
    return false;
}

void SoftwareContainerManager::setConfiguration(const QVariantMap &configuration)
{
    m_configuration = configuration;
}

ContainerInterface *SoftwareContainerManager::create(bool isQuickLaunch, const QVector<int> &stdioRedirections,
                                                     const QMap<QString, QString> &debugWrapperEnvironment,
                                                     const QStringList &debugWrapperCommand)
{
    if (!m_interface) {
        QString dbus = configuration().value(QStringLiteral("dbus")).toString();
        QDBusConnection conn(QStringLiteral("sc-bus"));

        if (dbus.isEmpty())
            dbus = QStringLiteral("system");

        if (dbus == QLatin1String("system"))
             conn = QDBusConnection::systemBus();
        else if (dbus == QLatin1String("session"))
            conn = QDBusConnection::sessionBus();
        else
            conn = QDBusConnection::connectToBus(dbus, QStringLiteral("sc-bus"));

        if (!conn.isConnected()) {
            qWarning() << "The" << dbus << "D-Bus is not available to connect to the SoftwareContainer agent.";
            return nullptr;
        }
        m_interface = new QDBusInterface(QStringLiteral("com.pelagicore.SoftwareContainerAgent"),
                                         QStringLiteral("/com/pelagicore/SoftwareContainerAgent"),
                                         QStringLiteral("com.pelagicore.SoftwareContainerAgent"),
                                         conn, this);
        if (m_interface->lastError().isValid()) {
            qWarning() << "Could not connect to com.pelagicore.SoftwareContainerAgent, "
                          "/com/pelagicore/SoftwareContainerAgent on the" << dbus << "D-Bus";
            delete m_interface;
            m_interface = nullptr;
            return nullptr;
        }

        if (!connect(m_interface, SIGNAL(ProcessStateChanged(int,uint,bool,uint)), this, SLOT(processStateChanged(int,uint,bool,uint)))) {
            qWarning() << "Could not connect to the com.pelagicore.SoftwareContainerAgent.ProcessStateChanged "
                          "signal on the" << dbus << "D-Bus";
            delete m_interface;
            m_interface = nullptr;
            return nullptr;
        }
    }

    QString config = QStringLiteral("[]");
    QVariant v = configuration().value(QStringLiteral("createConfig"));
    if (v.isValid())
        config = QString::fromUtf8(QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact));

    QDBusMessage reply = m_interface->call(QDBus::Block, QStringLiteral("Create"), config);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "SoftwareContainer failed to create a new container:" << reply.errorMessage()
                   << "(config was:" << config << ")";
        return nullptr;
    }

    int containerId = reply.arguments().at(0).toInt();

    if (containerId < 0) {
        qCritical() << "SoftwareContainer failed to create a new container. (config was:" << config << ")";
        return nullptr;
    }

    // calculate where to dump stdout/stderr
    // also close all file descriptors that we do not need
    int inFd = stdioRedirections.value(STDIN_FILENO, -1);
    int outFd = stdioRedirections.value(STDOUT_FILENO, -1);
    int errFd = stdioRedirections.value(STDERR_FILENO, -1);
    int outputFd = STDOUT_FILENO;

    if (inFd >= 0)
        ::close(inFd);
    if (errFd >= 0)
        outputFd = errFd;
    if (outFd >= 0) {
        if (errFd < 0)
            outputFd = outFd;
        else
            ::close(outFd);
    }

    SoftwareContainer *container = new SoftwareContainer(this, isQuickLaunch, containerId,
                                                         outputFd, debugWrapperEnvironment,
                                                         debugWrapperCommand);
    m_containers.insert(containerId, container);
    connect(container, &QObject::destroyed, this, [this, containerId]() { m_containers.remove(containerId); });
    return container;
}

QDBusInterface *SoftwareContainerManager::interface() const
{
    return m_interface;
}

QVariantMap SoftwareContainerManager::configuration() const
{
    return m_configuration;
}

void SoftwareContainerManager::processStateChanged(int containerId, uint processId, bool isRunning, uint exitCode)
{
    Q_UNUSED(processId)

    SoftwareContainer *container = m_containers.value(containerId);
    if (!container) {
        qWarning() << "Received a processStateChanged signal for unknown container" << containerId;
        return;
    }

    if (!isRunning)
        container->containerExited(exitCode);
}



SoftwareContainer::SoftwareContainer(SoftwareContainerManager *manager, bool isQuickLaunch, int containerId,
                                     int outputFd, const QMap<QString, QString> &debugWrapperEnvironment,
                                     const QStringList &debugWrapperCommand)
    : m_manager(manager)
    , m_isQuickLaunch(isQuickLaunch)
    , m_id(containerId)
    , m_outputFd(outputFd)
    , m_debugWrapperEnvironment(debugWrapperEnvironment)
    , m_debugWrapperCommand(debugWrapperCommand)
{ }

SoftwareContainer::~SoftwareContainer()
{
    if (m_fifoFd >= 0)
        QT_CLOSE(m_fifoFd);
    if (!m_fifoPath.isEmpty())
        ::unlink(m_fifoPath);
    if (m_outputFd > STDERR_FILENO)
        QT_CLOSE(m_outputFd);
}

SoftwareContainerManager *SoftwareContainer::manager() const
{
    return m_manager;
}

bool SoftwareContainer::attachApplication(const QVariantMap &application)
{

    // In normal launch attachApplication is called first, then the start()
    // method is called. During quicklaunch start() is called first and then
    // attachApplication. In this case we need to configure the container
    // with any extra capabilities etc.

    m_state = StartingUp;
    m_application = application;

    m_hostPath = application.value(QStringLiteral("codeDir")).toString();
    if (m_hostPath.isEmpty())
        m_hostPath = QDir::currentPath();

    m_appRelativeCodePath = application.value(QStringLiteral("codeFilePath")).toString();
    m_containerPath = QStringLiteral("/app");

    // If this is a quick launch instance, we need to renew the capabilities
    // and send the bindmounts.
    if (m_isQuickLaunch) {
        if (!sendCapabilities())
            return false;

        if (!sendBindMounts())
            return false;
    }

    m_ready = true;
    emit ready();
    return true;
}

QString SoftwareContainer::controlGroup() const
{
    return QString();
}

bool SoftwareContainer::setControlGroup(const QString &groupName)
{
    Q_UNUSED(groupName)
    return false;
}

bool SoftwareContainer::setProgram(const QString &program)
{
    m_program = program;
    return true;
}

void SoftwareContainer::setBaseDirectory(const QString &baseDirectory)
{
    m_baseDir = baseDirectory;
}

bool SoftwareContainer::isReady() const
{
    return m_ready;
}

QString SoftwareContainer::mapContainerPathToHost(const QString &containerPath) const
{
    return containerPath;
}

QString SoftwareContainer::mapHostPathToContainer(const QString &hostPath) const
{
    QString containerPath = m_containerPath;

    QFileInfo fileInfo(hostPath);
    if (fileInfo.isFile())
        containerPath = m_containerPath + QStringLiteral("/") + fileInfo.fileName();

    return containerPath;
}

bool SoftwareContainer::sendCapabilities()
{
    auto iface = manager()->interface();
    if (!iface)
        return false;

    // this is the one and only capability in io.qt.ApplicationManager.Application.json
    static const QStringList capabilities { QStringLiteral("io.qt.ApplicationManager.Application") };

    QDBusMessage reply = iface->call(QDBus::Block, QStringLiteral("SetCapabilities"), m_id, QVariant::fromValue(capabilities));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "SoftwareContainer failed to set capabilities to" << capabilities << ":" << reply.errorMessage();
        return false;
    }
    return true;
}

bool SoftwareContainer::sendBindMounts()
{
    auto iface = manager()->interface();
    if (!iface)
        return false;

    QFileInfo fontCacheInfo(QStringLiteral("/var/cache/fontconfig"));

    QVector<std::tuple<QString, QString, bool>> bindMounts; // bool == isReadOnly
    // the private P2P D-Bus
    bindMounts.append(std::make_tuple(m_dbusP2PInfo.absoluteFilePath(), m_dbusP2PInfo.absoluteFilePath(), false));

    // we need to share the fontconfig cache - otherwise the container startup might take a long time
    bindMounts.append(std::make_tuple(fontCacheInfo.absoluteFilePath(), fontCacheInfo.absoluteFilePath(), false));

    // Qt's plugin path
    bindMounts.append(std::make_tuple(m_qtPluginPathInfo.absoluteFilePath(), m_qtPluginPathInfo.absoluteFilePath(), false));

    // the actual path to the application
    bindMounts.append(std::make_tuple(m_hostPath, m_containerPath, true));

    // for development only - mount the user's $HOME dir into the container as read-only. Otherwise
    // you would have to `make install` the AM into /usr on every rebuild
    if (manager()->configuration().value(QStringLiteral("bindMountHome")).toBool())
        bindMounts.append(std::make_tuple(QDir::homePath(), QDir::homePath(), true));

    // do all the bind-mounts in parallel to waste as little time as possible
    QList<QDBusPendingReply<>> bindMountResults;

    for (auto it = bindMounts.cbegin(); it != bindMounts.cend(); ++it)
        bindMountResults << iface->asyncCall(QStringLiteral("BindMount"), m_id, std::get<0>(*it),
                                             std::get<1>(*it), std::get<2>(*it));

    for (const auto &pending : qAsConst(bindMountResults))
        QDBusPendingCallWatcher(pending).waitForFinished();

    for (int i = 0; i < bindMounts.size(); ++i) {
        if (bindMountResults.at(i).isError()) {
            qWarning() << "SoftwareContainer failed to bind-mount the directory" << std::get<0>(bindMounts.at(i))
                       << "into the container at" << std::get<1>(bindMounts.at(i)) << ":" << bindMountResults.at(i).error().message();
            return false;
        }
    }

    return true;

}

bool SoftwareContainer::start(const QStringList &arguments, const QMap<QString, QString> &runtimeEnvironment,
                              const QVariantMap &amConfig)
{
    auto iface = manager()->interface();
    if (!iface)
        return false;

    if (!QFile::exists(m_program))
        return false;

    // Send initial capabilities even if this is a quick-launch instance
    if (!sendCapabilities())
        return false;

    // parse out the actual socket file name from the DBus specification
    QString dbusP2PSocket = amConfig.value(QStringLiteral("dbus")).toMap().value(QStringLiteral("p2p")).toString();
    dbusP2PSocket = dbusP2PSocket.mid(dbusP2PSocket.indexOf(QLatin1Char('=')) + 1);
    dbusP2PSocket = dbusP2PSocket.left(dbusP2PSocket.indexOf(QLatin1Char(',')));
    QFileInfo dbusP2PInfo(dbusP2PSocket);
    m_dbusP2PInfo = dbusP2PInfo;

    // for bind-mounting the plugin-path later on
    QString qtPluginPath = runtimeEnvironment.value(QStringLiteral("QT_PLUGIN_PATH"));
    m_qtPluginPathInfo = QFileInfo(qtPluginPath);

    // Only send the bindmounts if this is not a quick-launch instance.
    if (!m_isQuickLaunch && !sendBindMounts())
        return false;

    // create an unique fifo name in /tmp
    m_fifoPath = QDir::tempPath().toLocal8Bit() + "/sc-" + QUuid::createUuid().toByteArray().mid(1,36) + ".fifo";
    if (mkfifo(m_fifoPath, 0600) != 0) {
        qWarning() << "Failed to create FIFO at" << m_fifoPath << "to redirect stdout/stderr out of the container:" << strerror(errno);
        m_fifoPath.clear();
        return false;
    }

    // open fifo for reading
    // due to QTBUG-15261 (bytesAvailable() on a fifo always returns 0) we use a raw fd
    m_fifoFd = QT_OPEN(m_fifoPath, O_RDONLY | O_NONBLOCK);
    if (m_fifoFd < 0) {
        qWarning() << "Failed to open FIFO at" << m_fifoPath << "for reading:" << strerror(errno);
        return false;
    }

    // read from fifo and dump to message handler
    QSocketNotifier *sn = new QSocketNotifier(m_fifoFd, QSocketNotifier::Read, this);
    int outputFd = m_outputFd;
    connect(sn, &QSocketNotifier::activated, this, [sn, outputFd](int fifoFd) {
        int bytesAvailable = 0;
        if (ioctl(fifoFd, FIONREAD, &bytesAvailable) == 0) {
            static const int bufferSize = 4096;
            static QByteArray buffer(bufferSize, 0);

            while (bytesAvailable > 0) {
                auto bytesRead = QT_READ(fifoFd, buffer.data(), std::min(bytesAvailable, bufferSize));
                if (bytesRead < 0) {
                    if (errno == EINTR || errno == EAGAIN)
                        continue;
                    sn->setEnabled(false);
                    return;
                } else if (bytesRead > 0) {
                    (void) QT_WRITE(outputFd, buffer.constData(), bytesRead);
                    bytesAvailable -= bytesRead;
                }
            }
        }
    });

    // Calculate the exact command to run
    QStringList command;
    if (!m_debugWrapperCommand.isEmpty()) {
        command = substituteCommand(m_debugWrapperCommand, m_program, arguments);
    } else {
        command = arguments;
        command.prepend(m_program);
    }

    // SC expects a plain string instead of individual args
    QString cmdLine;
    for (const auto &part : qAsConst(command)) {
        if (!cmdLine.isEmpty())
            cmdLine.append(QLatin1Char(' '));
        cmdLine.append(QLatin1Char('\"'));
        cmdLine.append(part);
        cmdLine.append(QLatin1Char('\"'));
    }
    //cmdLine.prepend(QStringLiteral("/usr/bin/strace ")); // useful if things go wrong...

    // we start with a copy of the AM's environment, but some variables would overwrite important
    // redirections set by SC gateways.
    static const QStringList forbiddenVars = {
        QStringLiteral("XDG_RUNTIME_DIR"),
        QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
        QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"),
        QStringLiteral("PULSE_SERVER")
    };

    // since we have to translate between a QProcessEnvironment and a QMap<>, we cache the result
    static QMap<QString, QString> baseEnvVars;
    if (baseEnvVars.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const auto keys = env.keys();
        for (const auto &key : keys) {
            if (!key.isEmpty() && !forbiddenVars.contains(key))
                baseEnvVars.insert(key, env.value(key));
        }
    }

    QMap<QString, QString> envVars = baseEnvVars;

    // set the env. variables coming from the runtime
    for (auto it = runtimeEnvironment.cbegin(); it != runtimeEnvironment.cend(); ++it) {
        if (it.value().isEmpty())
            envVars.remove(it.key());
        else
            envVars.insert(it.key(), it.value());
    }
    // set the env. variables coming from a debug wrapper
    for (auto it = m_debugWrapperEnvironment.cbegin(); it != m_debugWrapperEnvironment.cend(); ++it) {
        if (it.value().isEmpty())
            envVars.remove(it.key());
        else
            envVars.insert(it.key(), it.value());
    }

    QVariant venvVars = QVariant::fromValue(envVars);

    qDebug () << "SoftwareContainer is trying to launch application" << m_id
              << "\n * command ..." << cmdLine
              << "\n * directory ." << m_containerPath
              << "\n * output ...." << m_fifoPath
              << "\n * environment" << envVars;

#if 0
    qWarning() << "Attach to container now!";
    qWarning().nospace() << "  sudo lxc-attach -n SC-" << m_id;
    sleep(10000000);
#endif

    auto reply = iface->call(QDBus::Block, QStringLiteral("Execute"), m_id, cmdLine,
                             m_containerPath, QString::fromLocal8Bit(m_fifoPath), venvVars);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "SoftwareContainer failed to execute application" << m_id << "in directory"
                   << m_containerPath << "in the container:" << reply.errorMessage();
        return false;
    }

    m_pid = reply.arguments().at(0).value<int>();

    m_state = Running;
    QMetaObject::invokeMethod(this, [this]() {
        emit stateChanged(m_state);
        emit started();
    }, Qt::QueuedConnection);
    return true;
}

qint64 SoftwareContainer::processId() const
{
    return m_pid;
}

SoftwareContainer::RunState SoftwareContainer::state() const
{
    return m_state;
}

void SoftwareContainer::kill()
{
    auto iface = manager()->interface();

    if (iface) {
        QDBusMessage reply = iface->call(QDBus::Block, QStringLiteral("Destroy"), m_id);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "SoftwareContainer failed to destroy container" << reply.errorMessage();
        }

        if (!reply.arguments().at(0).toBool()) {
            qWarning() << "SoftwareContainer failed to destroy container.";
        }
    }
}

void SoftwareContainer::terminate()
{
    //TODO: handle graceful shutdown
    kill();
}

void SoftwareContainer::containerExited(uint exitCode)
{
    m_state = NotRunning;
    emit stateChanged(m_state);
    emit finished(WEXITSTATUS(exitCode), WIFEXITED(exitCode) ? NormalExit : CrashExit);
    deleteLater();
}

#include "moc_softwarecontainer.cpp"
