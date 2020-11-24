TEMPLATE = lib
TARGET = QtAppManMain
MODULE = appman_main

load(am-config)

QT = core network qml core-private gui quick
enable-widgets:QT *= widgets
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_package-private \
    appman_notification-private \
    appman_monitor-private \
    appman_shared_main-private \
    appman_intent_server-private \
    appman_window-private \

!disable-external-dbus-interfaces:qtHaveModule(dbus):QT *= dbus appman_dbus-private

CONFIG *= static internal_module
CONFIG -= create_cmake

win32:LIBS += -luser32

DEFINES += AM_BUILD_DIR=\\\"$$BUILD_DIR\\\"

HEADERS += \
    configuration.h \
    main.h \
    configuration_p.h \
    defaultconfiguration.h \
    applicationinstaller.h \
    windowframetimer.h \

SOURCES += \
    main.cpp \
    configuration.cpp \
    defaultconfiguration.cpp \
    applicationinstaller.cpp \
    windowframetimer.cpp \

load(qt_module)
