TEMPLATE = lib
TARGET = QtAppManMain
MODULE = appman_main

load(am-config)

QT = core network qml core-private
!headless:QT *= gui quick
qtHaveModule(dbus):QT *= dbus
qtHaveModule(pssdp):QT *= pssdp
qtHaveModule(pshellserver):QT *= pshellserver
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_package-private \
    appman_installer-private \
    appman_notification-private \
    appman_window-private \
    appman_monitor-private \

CONFIG *= static internal_module

win32:LIBS += -luser32

HEADERS += \
    $$PWD/qmllogger.h \
    $$PWD/configuration.h \
    $$PWD/main.h \
    $$PWD/defaultconfiguration.h

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/qmllogger.cpp \
    $$PWD/configuration.cpp \
    $$PWD/defaultconfiguration.cpp

DBUS_ADAPTORS += \
    $$PWD/../dbus/io.qt.applicationinstaller.xml \

!headless:DBUS_ADAPTORS += \
    $$PWD/../dbus/io.qt.windowmanager.xml \

# this is a bit more complicated than it should be, but qdbusxml2cpp cannot
# cope with more than 1 out value out of the box
# http://lists.qt-project.org/pipermail/interest/2013-July/008011.html
dbus-notifications.files = $$PWD/../dbus/org.freedesktop.notifications.xml
dbus-notifications.source_flags = -l QtAM::NotificationManager
dbus-notifications.header_flags = -l QtAM::NotificationManager -i notificationmanager.h

dbus-appman.files = $$PWD/../dbus/io.qt.applicationmanager.xml
dbus-appman.source_flags = -l QtAM::ApplicationManager
dbus-appman.header_flags = -l QtAM::ApplicationManager -i applicationmanager.h

DBUS_ADAPTORS += dbus-notifications dbus-appman

load(qt_module)
