
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
manager.depends = manager-lib installer-lib
launchers.depends = manager
tools.depends = package-lib

!tools-only {
    SUBDIRS += \
        plugin-interfaces \
        dbus \

    qtHaveModule(qml):SUBDIRS += \
        notification-lib \
        manager-lib \
        installer-lib \
        manager \
        launchers

    tools.depends = manager-lib installer-lib
}

SUBDIRS += tools
