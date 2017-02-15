TEMPLATE = lib
TARGET = QtAppManManager
MODULE = appman_manager

load(am-config)

QT = core network qml
!headless:QT *= gui quick
qtHaveModule(dbus):QT *= dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \
    appman_notification-private \
    appman_plugininterfaces-private \

CONFIG *= static internal_module

multi-process {
    PKGCONFIG += "'dbus-1 >= 1.6'"
}

HEADERS += \
    applicationmanager.h \
    applicationdatabase.h \
    notificationmanager.h \
    abstractcontainer.h \
    containerfactory.h \
    plugincontainer.h \
    processcontainer.h \
    abstractruntime.h \
    runtimefactory.h \
    quicklauncher.h \
    applicationipcmanager.h \
    applicationipcinterface.h \
    applicationipcinterface_p.h \
    systemmonitor.h \
    systemmonitor_p.h \
    processmonitor.h \
    memorymonitor.h \
    fpsmonitor.h \
    applicationmanager_p.h

!headless:HEADERS += \
    fakeapplicationmanagerwindow.h \

multi-process:HEADERS += \
    nativeruntime.h \
    nativeruntime_p.h \

linux:HEADERS += \
    sysfsreader.h \

qtHaveModule(qml):HEADERS += \
    qmlinprocessruntime.h \
    qmlinprocessapplicationinterface.h \

SOURCES += \
    applicationmanager.cpp \
    applicationdatabase.cpp \
    notificationmanager.cpp \
    abstractcontainer.cpp \
    containerfactory.cpp \
    plugincontainer.cpp \
    processcontainer.cpp \
    abstractruntime.cpp \
    runtimefactory.cpp \
    quicklauncher.cpp \
    applicationipcmanager.cpp \
    applicationipcinterface.cpp \
    systemmonitor.cpp \
    systemmonitor_p.cpp \
    processmonitor.cpp \
    memorymonitor.cpp \
    fpsmonitor.cpp \

!headless:SOURCES += \
    fakeapplicationmanagerwindow.cpp \

multi-process:SOURCES += \
    nativeruntime.cpp \

linux:SOURCES += \
    sysfsreader.cpp \

qtHaveModule(qml):SOURCES += \
    qmlinprocessruntime.cpp \
    qmlinprocessapplicationinterface.cpp \

# we have an external plugin interface with signals, so we need to
# compile the moc-data into the exporting binary (appman itself)
HEADERS += ../plugin-interfaces/containerinterface.h

load(qt_module)
