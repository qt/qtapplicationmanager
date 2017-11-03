TEMPLATE = lib
TARGET = QtAppManMain
MODULE = appman_main

load(am-config)

QT = core network qml core-private
enable-widgets:QT *= widgets
!headless:QT *= gui quick
qtHaveModule(pssdp):QT *= pssdp
qtHaveModule(pshellserver):QT *= pshellserver
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_package-private \
    appman_installer-private \
    appman_notification-private \
    appman_monitor-private \

!headless:QT *= appman_window-private
!disable-external-dbus-interfaces:qtHaveModule(dbus):QT *= dbus appman_dbus-private

CONFIG *= static internal_module

win32:LIBS += -luser32

DEFINES += AM_BUILD_DIR=\\\"$$BUILD_DIR\\\"

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

load(qt_module)
