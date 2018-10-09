TEMPLATE = lib
TARGET = QtAppManDBus
MODULE = appman_dbus

load(am-config)

QT = core dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_manager-private \

CONFIG *= static internal_module
CONFIG = dbus-adaptors-xml $$CONFIG

HEADERS += \
    dbuspolicy.h \
    dbusdaemon.h \
    abstractdbuscontextadaptor.h \
    applicationmanagerdbuscontextadaptor.h \
    notificationmanagerdbuscontextadaptor.h \

SOURCES += \
    dbuspolicy.cpp \
    dbusdaemon.cpp \
    abstractdbuscontextadaptor.cpp \
    applicationmanagerdbuscontextadaptor.cpp \
    notificationmanagerdbuscontextadaptor.cpp \

ADAPTORS_XML = \
    io.qt.applicationmanager.xml \
    org.freedesktop.notifications.xml \

!disable-installer {
    QT *= appman_installer-private
    HEADERS += applicationinstallerdbuscontextadaptor.h
    SOURCES += applicationinstallerdbuscontextadaptor.cpp
    ADAPTORS_XML += io.qt.applicationinstaller.xml
}

!headless{
    QT *= appman_window-private
    HEADERS += windowmanagerdbuscontextadaptor.h
    SOURCES += windowmanagerdbuscontextadaptor.cpp
    ADAPTORS_XML += io.qt.windowmanager.xml
}

OTHER_FILES = \
    io.qt.applicationinstaller.xml \
    io.qt.applicationmanager.applicationinterface.xml \
    io.qt.applicationmanager.runtimeinterface.xml \
    io.qt.applicationmanager.xml \
    io.qt.windowmanager.xml \
    org.freedesktop.notifications.xml \

qtPrepareTool(QDBUSCPP2XML, qdbuscpp2xml)

recreate-applicationmanager-dbus-xml.CONFIG = phony
recreate-applicationmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/applicationmanager.h -o $$PWD/io.qt.applicationmanager.xml

recreate-applicationinstaller-dbus-xml.CONFIG = phony
recreate-applicationinstaller-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../installer-lib/applicationinstaller.h -o $$PWD/io.qt.applicationinstaller.xml

recreate-windowmanager-dbus-xml.CONFIG = phony
recreate-windowmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager/windowmanager.h -o $$PWD/io.qt.windowmanager.xml

recreate-dbus-xml.depends = recreate-applicationmanager-dbus-xml recreate-applicationinstaller-dbus-xml recreate-windowmanager-dbus-xml

QMAKE_EXTRA_TARGETS += \
    recreate-dbus-xml \
    recreate-applicationmanager-dbus-xml \
    recreate-applicationinstaller-dbus-xml \
    recreate-windowmanager-dbus-xml \

load(qt_module)
