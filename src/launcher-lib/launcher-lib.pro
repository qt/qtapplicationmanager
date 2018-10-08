TEMPLATE = lib
TARGET = QtAppManLauncher
MODULE = appman_launcher

load(am-config)

QT = qml dbus core-private
enable-widgets:QT *= widgets
!headless:QT += quick gui gui-private quick-private
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_shared_main-private \
    appman_application-private \
    appman_notification-private \
    appman_intent_client-private \

CONFIG *= static internal_module

DBUS_INTERFACES += ../dbus-lib/io.qt.applicationmanager.intentinterface.xml

SOURCES += \
    qmlapplicationinterface.cpp \
    ipcwrapperobject.cpp \
    qmlapplicationinterfaceextension.cpp \
    qmlnotification.cpp \
    launchermain.cpp \
    intentclientdbusimplementation.cpp

!headless:SOURCES += \
    applicationmanagerwindow.cpp \

!headless:qtHaveModule(waylandclient) {
    QT *= waylandclient waylandclient-private
    CONFIG *= wayland-scanner generated_privates
    private_headers.CONFIG += no_check_exists
    WAYLANDCLIENTSOURCES += ../wayland-extensions/qtam-extension.xml
    HEADERS += waylandqtamclientextension_p.h
    SOURCES += waylandqtamclientextension.cpp
}

HEADERS += \
    qmlapplicationinterface.h \
    ipcwrapperobject.h \
    ipcwrapperobject_p.h \
    qmlapplicationinterfaceextension.h \
    qmlnotification.h \
    launchermain.h \
    intentclientdbusimplementation.h

!headless:HEADERS += \
    applicationmanagerwindow_p.h

load(qt_module)
