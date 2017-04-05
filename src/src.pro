
TEMPLATE = subdirs

load(am-config)

SUBDIRS = \
    common-lib \
    crypto-lib \
    application-lib \
    package-lib \

crypto-lib.depends = common-lib
application-lib.depends = crypto-lib
package-lib.depends = crypto-lib application-lib
notification-lib.depends = common-lib
manager-lib.depends = application-lib notification-lib plugin-interfaces
installer-lib.depends = package-lib manager-lib
window-lib.depends = manager-lib
monitor-lib.depends = manager-lib window-lib
launcher-lib.depends = application-lib notification-lib
manager.depends = manager-lib installer-lib window-lib monitor-lib
launchers.depends = launcher-lib
tools.depends = package-lib

!tools-only {
    SUBDIRS += \
        plugin-interfaces \
        dbus \

    qtHaveModule(qml):SUBDIRS += \
        notification-lib \
        manager-lib \
        installer-lib \
        window-lib \
        manager \
        monitor-lib \
        launcher-lib \
        launchers

    tools.depends = manager-lib installer-lib launcher-lib window-lib monitor-lib
}

SUBDIRS += tools
