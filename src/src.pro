
TEMPLATE = subdirs

load(am-config)

SUBDIRS = \
    common-lib \
    crypto-lib \
    application-lib \
    package-lib \
    tools \
    plugin-interfaces \

qtHaveModule(qml):SUBDIRS += \
    notification-lib \
    manager-lib \
    installer-lib \
    manager \
    launchers \

crypto-lib.depends = common-lib
application-lib.depends = crypto-lib
package-lib.depends = crypto-lib application-lib
notification-lib.depends = common-lib
manager-lib.depends = application-lib notification-lib
installer-lib.depends = package-lib manager-lib
manager.depends = manager-lib installer-lib
launchers.depends = manager

tools.depends = package-lib

OTHER_FILES = \
    dbus/*
