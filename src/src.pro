
TEMPLATE = subdirs

load(am-config)

SUBDIRS = \
    common-lib \
    crypto-lib \
    manager-lib \
    notification-lib \
    installer-lib \
    launchers \
    manager \
    tools \
    plugin-interfaces \

crypto-lib.depends = common-lib
manager-lib.depends = common-lib crypto-lib
manager-lib.depends += notification-lib
notification-lib.depends = common-lib
installer-lib.depends = common-lib crypto-lib manager-lib
manager.depends = manager-lib installer-lib
launchers.depends = manager
tools.depends = installer-lib

OTHER_FILES = \
    dbus/*
