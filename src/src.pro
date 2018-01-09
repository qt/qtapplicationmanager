
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

shared_main_lib.subdir = shared-main-lib
shared_main_lib.depends = common_lib

launcher_lib.subdir = launcher-lib
launcher_lib.depends = application_lib notification_lib shared_main_lib

main_lib.subdir = main-lib
main_lib.depends = shared_main_lib manager_lib installer_lib window_lib monitor_lib

!disable-external-dbus-interfaces:qtHaveModule(dbus) {
    dbus_lib.subdir = dbus-lib
    dbus_lib.depends = manager_lib installer_lib window_lib

    main_lib.depends += dbus_lib
}

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

    !disable-external-dbus-interfaces:qtHaveModule(dbus):SUBDIRS += \
        dbus_lib \

    qtHaveModule(qml):SUBDIRS += \
        notification_lib \
        manager_lib \
        installer_lib \
        window_lib \
        monitor_lib \
        shared_main_lib \
        main_lib \
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

qtHaveModule(dbus):SUBDIRS += \
    tools_controller \
