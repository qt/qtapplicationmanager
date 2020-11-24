TEMPLATE = lib
TARGET = QtAppManDBus
MODULE = appman_dbus

load(am-config)

QT = core dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_manager-private \
    appman_window-private \

CONFIG *= static internal_module
CONFIG -= create_cmake
CONFIG = dbus-adaptors-xml $$CONFIG

HEADERS += \
    dbuspolicy.h \
    dbusdaemon.h \
    abstractdbuscontextadaptor.h \
    applicationmanagerdbuscontextadaptor.h \
    notificationmanagerdbuscontextadaptor.h \
    windowmanagerdbuscontextadaptor.h \

SOURCES += \
    dbuspolicy.cpp \
    dbusdaemon.cpp \
    abstractdbuscontextadaptor.cpp \
    applicationmanagerdbuscontextadaptor.cpp \
    notificationmanagerdbuscontextadaptor.cpp \
    windowmanagerdbuscontextadaptor.cpp \

ADAPTORS_XML = \
    io.qt.applicationmanager.xml \
    io.qt.windowmanager.xml \
    org.freedesktop.notifications.xml \

!disable-installer {
    HEADERS += packagemanagerdbuscontextadaptor.h
    SOURCES += packagemanagerdbuscontextadaptor.cpp
    ADAPTORS_XML += io.qt.packagemanager.xml
}

OTHER_FILES = \
    io.qt.packagemanager.xml \
    io.qt.applicationmanager.applicationinterface.xml \
    io.qt.applicationmanager.runtimeinterface.xml \
    io.qt.applicationmanager.intentinterface.xml \
    io.qt.applicationmanager.xml \
    io.qt.windowmanager.xml \
    org.freedesktop.notifications.xml \

qtPrepareTool(QDBUSCPP2XML, qdbuscpp2xml)

recreate-applicationmanager-dbus-xml.CONFIG = phony
recreate-applicationmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/applicationmanager.h -o $$PWD/io.qt.applicationmanager.xml

recreate-packagemanager-dbus-xml.CONFIG = phony
recreate-packagemanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/packagemanager.h -o $$PWD/io.qt.packagemanager.xml

recreate-windowmanager-dbus-xml.CONFIG = phony
recreate-windowmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager/windowmanager.h -o $$PWD/io.qt.windowmanager.xml

recreate-dbus-xml.depends = recreate-applicationmanager-dbus-xml recreate-applicationinstaller-dbus-xml recreate-windowmanager-dbus-xml

QMAKE_EXTRA_TARGETS += \
    recreate-dbus-xml \
    recreate-applicationmanager-dbus-xml \
    recreate-packagemanager-dbus-xml \
    recreate-windowmanager-dbus-xml \

load(qt_module)
