// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "containerinterface.h"

ContainerInterface::ContainerInterface() { }
ContainerInterface::~ContainerInterface() { }

ContainerManagerInterface::ContainerManagerInterface() { }
ContainerManagerInterface::~ContainerManagerInterface() { }

bool ContainerManagerInterface::initialize(ContainerHelperFunctions *) { return true; }

/*! \class ContainerInterface
    \inmodule QtApplicationManager
    \brief An interface for custom container instances.

    A pointer to this interface should be returned from a successful call to
    ContainerManagerInterface::create()

    Each instance of this class corresponds to a single application or - if supported - a single
    quick-launcher instance.

    The interface is closely modelled after QProcess and even reuses many of QProcess' enums.
*/

/*! \enum ContainerInterface::ExitStatus
    This enum describes the different exit statuses of an application.

    \value NormalExit The application exited normally.
    \value CrashExit  The application crashed.
    \value ForcedExit The application was killed by the application manager, since it ignored the
                      quit request originating from a call to ApplicationManager::stopApplication.

    \sa ApplicationObject::lastExitStatus, QProcess::ExitStatus
*/

/*! \enum ContainerInterface::RunState
    This enum describes the different run states of an application.

    \value NotRunning   The application has not been started yet.
    \value StartingUp   The application has been started and is initializing.
    \value Running      The application is running.
    \value ShuttingDown The application has been stopped and is cleaning up (in multi-process mode
                        this state is only reached if the application is terminating gracefully).

    \sa ApplicationObject::runState, QProcess::ProcessState
*/

/*! \enum ContainerInterface::ProcessError
    This enum describes the different types of errors that are reported by an application.

    Its names and values are an exact copy of QProcess::ProcessError.

    \omitvalue FailedToStart
    \omitvalue Crashed
    \omitvalue Timedout
    \omitvalue WriteError
    \omitvalue ReadError
    \omitvalue UnknownError
*/

/*! \fn bool ContainerInterface::attachApplication(const QVariantMap &application)

    The application manager calls this function, when an \a application has to be attached to this
    container instance: the \a application map closely corresponds to the application's meta-data
    returned from the ApplicationManager::get() method:

    These fields are \b not available for this function:
    \table
    \header
      \li Name
    \row \li isRunning
    \row \li isStartingUp
    \row \li isShuttingDown
    \row \li isLocked
    \row \li isUpdating
    \row \li isRemovable
    \row \li updateProgress
    \row \li application
    \row \li lastExitCode
    \row \li lastExitStatus
    \endtable

    These fields are \b added on top of what ApplicationManager::get() provides:
    \table
    \header
      \li Name
      \li Description
    \row
      \li \c codeDir
      \li The directory where the application's code is located.
    \row
      \li \c manifestDir
      \li The directory where the application's manifest (\c info.yaml) is located.
    \row
      \li \c applicationProperties
      \li A map with all application properties as seen in the manifest.
    \endtable
*/

/*! \fn QString ContainerInterface::controlGroup() const

    This function needs to return which control group the application's container is in. The control
    group name returned by this function is not the low-level name used by the operating
    system, but an abstraction. See the \l{control group mapping}{container documentation} for more
    details on how to setup this mapping.

    \note If your custom container solution does not support cgroups, then simply return an empty
          QString here.
*/

/*! \fn bool ContainerInterface::setControlGroup(const QString &groupName)

    This function is expected to move the application's container into a control group. The
    \a groupName is not the low-level name used by the operating system, but an abstraction. See
    the \l{control group mapping}{container documentation} for more  details on how to setup this
    mapping.
    Returns \c true if the call was successful or \c false otherwise.

    \note If your custom container solution does not support cgroups, then simply return \c false
          here.
*/

/*! \fn bool ContainerInterface::setProgram(const QString &program)
    This function is called by the application manager in order to set the \a program to execute
    when starting the process. This function will be called before start().
    Needs to return \c true if \a program is valid within this container or \c false otherwise.

    \sa start()
*/

/*! \fn void ContainerInterface::setBaseDirectory(const QString &baseDirectory)
    Called by the application manager to set the initial working directory for the process within
    the container to \a baseDirectory.
*/

/*! \fn bool ContainerInterface::isReady() const
    Needs to return \c true if the container is ready for starting the application's program or
    \c false otherwise.

    \sa ready()
*/

/*! \fn QString ContainerInterface::mapContainerPathToHost(const QString &containerPath) const
    The application manager will call this function whenever paths have to be mapped between the
    filesystem namespace of the application's container and the application manager.

    You can simply return \a containerPath, if both are running in the same namespace.
*/

/*! \fn QString ContainerInterface::mapHostPathToContainer(const QString &hostPath) const
    The application manager will call this function whenever paths have to be mapped between the
    filesystem namespace of the application manager and the application's container.

    You can simply return \a hostPath, if both are running in the same namespace.
*/

/*! \fn bool ContainerInterface::start(const QStringList &arguments,
                                       const QMap<QString, QString> &runtimeEnvironment,
                                       const QVariantMap &amConfig)
    This function will be called to asynchronously start the application's program (as set by
    setProgram()), with the additional command line \a arguments and with the additional environment
    variables from \a runtimeEnvironment.

    The \a runtimeEnvironment is a string map for environment variables and their values. An empty
    value in this map means, that the environment variable denoted by its key shall be unset. The
    native runtime will define these variables by default:

    \table
    \header
      \li Name
      \li Description
    \row
      \li \c QT_QPA_PLATFORM
      \li Set to \c wayland.
    \row
      \li \c QT_IM_MODULE
      \li Empty (unset), which results in applications using the default wayland text input method.
    \row
      \li \c QT_SCALE_FACTOR
      \li Empty (unset), to prevent scaling of wayland clients relative to the compositor. Otherwise
          running the application manager on a 4K desktop with scaling would result in double-scaled
          applications within the application manager.
    \row
      \li \c QT_WAYLAND_SHELL_INTEGRATION
      \li Set to \c xdg-shell. This is the preferred wayland shell integration.
    \row
      \li \c AM_CONFIG
      \li A YAML, UTF-8 string encoded version of the \a amConfig map (see below).
    \row
      \li \c AM_NO_DLT_LOGGING
      \li Only set to \c 1, if DLT logging is to be switched off (otherwise not set at all).
          The same information is available via \a amConfig (see below), but some applications and
          launchers may need this information as early as possible.
    \endtable

    The \a amConfig map is a collection of settings that are communicated to the program from the
    application manager. The same information is already string encoded in the \c AM_CONFIG
    environment variable within \a runtimeEnvironment, but it would be tedious to reparse that YAML
    fragment in the container plugin.

    \target amConfigDetails
    These are the currently defined fields in amConfig:

    \table
    \header
      \li Name
      \li Type
      \li Description
    \row
      \li \c baseDir
      \li string
      \li The base directory from where the application manager was started. Needed for relative
          import paths.
    \row
      \li \c dbus/p2p
      \li string
      \li The D-Bus address for the peer-to-peer bus between the application manager and this
          application.
    \row
      \li \c dbus/org.freedesktop.Notifications
      \li string
      \li The D-Bus address for the notification interface. Can either be \c session (default),
          \c system or an actual D-Bus bus address.
    \row
      \li \c logging/dlt
      \li bool
      \li Logging should be done via DLT, if this is set to \c true.
    \row
      \li \c logging/rules
      \li array<string>
      \li The Qt logging rules as set in the application manager's main config.
    \row
      \li \c systemProperties
      \li object
      \li The project specific \l{system properties} that were set via the application manager's
          main config file.
    \row
      \li \c runtimeConfiguration
      \li object
      \li The \l {Runtime configuration} used for this application.
    \row
      \li \c ui/slowAnimations
      \li bool
      \li Set to \c true, if animations should be slowed down.
    \row
      \li \c ui/iconThemeName
      \li string
      \li If set in the application manager, this will forward the icon theme name to the application
          (see QIcon::setThemeName()).
    \row
      \li \c ui/iconThemeSearchPaths
      \li string
      \li If set in the application manager, this will forward the icon theme search paths to the
          application (see QIcon::setThemeSearchPaths()).
    \row
      \li \c ui/opengl
      \li object
      \li If a specific OpenGL configuration is requested either globally for the application manager
          or just for this application (via its \c info.yaml manifest), this field will contain the
          \l{OpenGL Specification}{required OpenGL configuration}.
    \endtable

    The application manager will only ever call this function once for any given instance.

    This function should return \c true in case it succeeded or \c false otherwise. In case it
    returns \c true, the implementation needs to either emit the started() or
    \l{QProcess::errorOccurred()}{errorOccurred()} signal (can be delayed) in response to this
    call.

    \sa QProcess::start()
*/

/*! \fn qint64 ContainerInterface::processId() const

    Should return the native process identifier for the running process, if available. If no process
    is currently running, \c 0 is returned.

    \note The process identifier needs to be translated into the application manager's PID
          namespace, if the container solution is managing its own, private namespace.
    \note This identifier is needed to support security features within the application manager:
          Having a valid PID for every container is necessary (a) in order for Wayland windows to
          only be accepted when coming from known PIDs and (b) for system services to retrieve
          meta-data on applications via the D-Bus call ApplicationManager::identifyApplication().
          If you need a quick workaround during development for Wayland to accept windows from any
          PID, then run the application manager with \c{--no-security}.
*/

/*! \fn QProcess::ProcessState ContainerInterface::state() const

    This function needs to return the current state of the process within the container.

    \sa stateChanged(), errorOccured(), QProcess::state()
*/

/*! \fn void ContainerInterface::kill()

    Called by the application manager, if it wants to kill the current process within the
    container, causing it to exit immediately.

    On Unix, the equivalent would be sending a \c SIGKILL signal.

    \sa terminate()
*/

/*! \fn void ContainerInterface::terminate()

    Called by the application manager, when attempting to terminate the process within the container.

    The process may not exit as a result of calling this function.
    On Unix, the equivalent would be sending a \c SIGTERM signal.

    \sa kill()
*/


/*! \fn void ContainerInterface::ready()

    If the container implementation needs to do potentially expensive initialization work in the
    background, it can tell the application manager to delay calling start() until it has emitted
    this signal.

    In case this is not needed, then you never have to emit this signal, but you are expected to
    always return \c true from isReady().

    \sa isReady()
*/

/*! \fn void ContainerInterface::started()

    This signal has to be emitted when the process within the container has started and state()
    returns QProcess::Running.

    \sa QProcess::started()
*/

/*! \fn void ContainerInterface::errorOccured(ProcessError processError)

    This signal needs to be emitted when an error occurs with the process within the container. The
    specified \a processError describes the type of error that occurred.

    \sa QProcess::errorOccurred()
*/

/*! \fn void ContainerInterface::finished(int exitCode, ExitStatus exitStatus)

    This signal has to be emitted when the process within the container finishes. The \a exitStatus
    parameter gives an indication why the process exited - in case of a normal exit, the \a exitCode
    will be the return value of the process' \c main() function.

    \sa QProcess::finished()
*/

/*! \fn void ContainerInterface::stateChanged(RunState state)

    This signal needs to be emitted whenever the \a state of the process within the container changes.
*/


/*! \class ContainerManagerInterface
    \inmodule QtApplicationManager
    \brief A plugin interface for custom container solutions.

    This is the interface that you have to implement, if you want to get your own custom container
    solution into the application manager.

    For an example, please see \c{examples/software-container}. This example shows the integration
    of Pelagicore's SoftwareContainers (which are essentially nice-to-use LXC wrappers).
*/

/*! \fn void ContainerManagerInterface::initialize(ContainerHelperFunctions *helpers)
    This function is called by the application manager right after your plugin has been loaded,
    your interface has been instantiated and the configuration was set via setConfiguration.

    The \a helpers interface gives you access to common functionality that can be shared between
    different container plugin implementations. The pointer is owned by the application manager and
    is valid during the lifetime of the interface.

    Return \c true if your plugin is usable, given its configuration and system state. If not,
    then return \c false and the application manager will disable this container plugin.
*/

/*! \fn QString ContainerManagerInterface::identifier() const
    Should return the unique identifier string for this container plugin.
*/

/*! \fn bool ContainerManagerInterface::supportsQuickLaunch() const
    Expected to return \c true if the interface has support for quick-launching or \c false otherwise.
*/

/*! \fn void ContainerManagerInterface::setConfiguration(const QVariantMap &configuration)
    Called by the application manager after parsing its configuration files. The \a configuration
    map corresponds to the (optional) container specific configuration as described in the
    \l{Containers}{container documentation}.
*/

/*! \fn ContainerInterface *ContainerManagerInterface::create(bool isQuickLaunch, const QVector<int> &stdioRedirections, const QMap<QString, QString> &debugWrapperEnvironment, const QStringList &debugWrapperCommand)
    The application manager will call this function every time it needs to create a specific
    container for a direct application launch, or a runtime quick-launcher (depending on the value
    of the \a isQuickLaunch parameter).

    If the \a stdioRedirections vector is not empty, the plugin should - if possible - redirect
    standard-io streams: the vector can have 3 entries at most, with the index corresponding to the
    Unix standard-io file number (0: \c stdin, 1: \c stdout and 2: \c stderr). The values in this
    vector are either open OS file descriptors for redirections or \c -1.
    \c{[-1, 5, 5]} would mean: ignore \c stdin and redirect both \c stdout and \c stderr to fd \c 5.

    The ownership of these file descriptors is transferred if, and only if, a new ContainerInterface
    is successfully instantiated (i.e. the return value is not nullptr). They then have to be closed
    either immediately, in case this plugin is not able to use them, or latest, when the started
    application has finished.

    The \a debugWrapperEnvironment is an optional string map for environment variables and their
    values, if a debug-wrapper is to be used (the \a debugWrapperCommand is not empty - see below).
    An empty value in this map means, that the environment variable denoted by its key shall be
    unset.

    In case the \a debugWrapperCommand is not empty, the plugin is requested to execute the binary
    set by ContainterInterface::setProgram using this debug-wrapper. The plugin is responsible for
    combining both and for handling the replacement of \c{%program%} and \c{%arguments%}. See the
    \l{DebugWrappers} {debug-wrapper documentation} for more information.
*/
