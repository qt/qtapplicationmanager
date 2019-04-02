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
    appman_notification-private \
    appman_monitor-private \
    appman_shared_main-private \
    appman_intent_server-private \
    appman_installer-private \

!headless:QT *= appman_window-private
!disable-external-dbus-interfaces:qtHaveModule(dbus):QT *= dbus appman_dbus-private

CONFIG *= static internal_module
CONFIG -= create_cmake

win32:LIBS += -luser32

DEFINES += AM_BUILD_DIR=\\\"$$BUILD_DIR\\\"

HEADERS += \
    configuration.h \
    main.h \
    defaultconfiguration.h \

!headless:HEADERS += \
    windowframetimer.h \
    applicationinstaller.h \

SOURCES += \
    main.cpp \
    configuration.cpp \
    defaultconfiguration.cpp \

!headless:SOURCES += \
    windowframetimer.cpp \
    applicationinstaller.cpp \

load(qt_module)
