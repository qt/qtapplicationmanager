
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
manager_lib.depends = application_lib notification_lib intent_server_lib intent_client_lib monitor_lib plugin_interfaces
!disable-installer:manager_lib.depends += package_lib crypto_lib

window_lib.subdir = window-lib
window_lib.depends = manager_lib

monitor_lib.subdir = monitor-lib
monitor_lib.depends = common_lib

shared_main_lib.subdir = shared-main-lib
shared_main_lib.depends = common_lib monitor_lib

intent_server_lib.subdir = intent-server-lib
intent_server_lib.depends = common_lib

intent_client_lib.subdir = intent-client-lib
intent_client_lib.depends = common_lib

launcher_lib.subdir = launcher-lib
launcher_lib.depends = application_lib notification_lib shared_main_lib intent_client_lib

main_lib.subdir = main-lib
main_lib.depends = shared_main_lib manager_lib window_lib monitor_lib

!disable-external-dbus-interfaces:qtHaveModule(dbus) {
    dbus_lib.subdir = dbus-lib
    dbus_lib.depends = manager_lib window_lib

    main_lib.depends += dbus_lib
}

tools_launcher_qml.subdir = tools/launcher-qml
tools_launcher_qml.depends = launcher_lib plugin_interfaces monitor_lib

tools_appman.subdir = tools/appman
tools_appman.depends = main_lib

tools_testrunner.subdir = tools/testrunner
tools_testrunner.depends = main_lib

tools_dumpqmltypes.subdir = tools/dumpqmltypes
tools_dumpqmltypes.depends = manager_lib intent_server_lib window_lib shared_main_lib launcher_lib main_lib

tools_packager.subdir = tools/packager
tools_packager.depends = package_lib application_lib crypto_lib

tools_uploader.subdir = tools/uploader
tools_uploader.depends = common_lib

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
        window_lib \
        monitor_lib \
        shared_main_lib \
        intent_server_lib \
        intent_client_lib \
        main_lib \
        tools_appman \
        # Although the testrunner is in tools we don't want to build it with tools-only
        # because it is based on the manager binary
        tools_testrunner \

    qtHaveModule(qml):qtHaveModule(dbus):SUBDIRS += \
        launcher_lib \

    # This tool links against everything to extract the Qml type information
    !disable-installer:qtHaveModule(qml):qtHaveModule(dbus):!headless:SUBDIRS += \
        tools_dumpqmltypes \

    multi-process:qtHaveModule(qml):qtHaveModule(dbus):SUBDIRS += \
        tools_launcher_qml \
}

!cross_compile | tools-only {
    !disable-installer:SUBDIRS += \
        tools_packager

    SUBDIRS += \
        tools_uploader
}

qtHaveModule(dbus):SUBDIRS += \
    tools_controller \
