
TEMPLATE = subdirs

load(am-config)

common_lib.subdir = common-lib

plugin_interfaces.subdir = plugin-interfaces

crypto_lib.subdir = crypto-lib
crypto_lib.depends = common_lib

application_lib.subdir = application-lib
application_lib.depends = common_lib

notification_lib.subdir = notification-lib
notification_lib.depends = common_lib

package_lib.subdir = package-lib
package_lib.depends = crypto_lib application_lib

manager_lib.subdir = manager-lib
manager_lib.depends = application_lib notification_lib plugin_interfaces

installer_lib.subdir = installer-lib
installer_lib.depends = package_lib manager_lib

window_lib.subdir = window-lib
window_lib.depends = manager_lib

monitor_lib.subdir = monitor-lib
monitor_lib.depends = manager_lib window_lib

launcher_lib.subdir = launcher-lib
launcher_lib.depends = application_lib notification_lib

main_lib.subdir = main-lib
main_lib.depends = manager_lib installer_lib window_lib monitor_lib

launchers_qml.subdir = launchers/qml
launchers_qml.depends = launcher_lib plugin_interfaces

tools_appman.subdir = tools/appman
tools_appman.depends = main_lib

tools_testrunner.subdir = tools/testrunner
tools_testrunner.depends = main_lib

tools_dumpqmltypes.subdir = tools/dumpqmltypes
tools_dumpqmltypes.depends = manager_lib installer_lib window_lib monitor_lib launcher_lib

tools_packager.subdir = tools/packager
tools_packager.depends = package_lib

tools_deployer.subdir = tools/deployer

tools_controller.subdir = tools/controller
tools_controller.depends = common_lib

SUBDIRS = \
    common_lib \
    crypto_lib \
    application_lib \
    package_lib \

!tools-only {
    SUBDIRS += \
        plugin_interfaces \
        dbus \

    qtHaveModule(qml):SUBDIRS += \
        notification_lib \
        manager_lib \
        installer_lib \
        window_lib \
        main_lib \
        monitor_lib \
        tools_appman \
        # Although the testrunner is in tools we don't want to build it with tools-only
        # because it is based on the manager binary
        tools_testrunner \

    qtHaveModule(qml):qtHaveModule(dbus):SUBDIRS += \
        launcher_lib \
        # This tool links against everything to extract the Qml type information
        tools_dumpqmltypes \

    multi-process:qtHaveModule(qml):qtHaveModule(dbus):SUBDIRS += \
        launchers_qml \
}

!android:SUBDIRS += \
    tools_packager \
    tools_deployer \

qtHaveModule(dbus):SUBDIRS += \
    tools_controller \
