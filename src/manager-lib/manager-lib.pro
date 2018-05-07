TEMPLATE = lib
TARGET = QtAppManManager
MODULE = appman_manager

load(am-config)

QT = core network qml
!headless:QT *= gui gui-private quick qml-private
qtHaveModule(dbus):QT *= dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_application-private \
    appman_notification-private \
    appman_plugininterfaces-private \

CONFIG *= static internal_module

multi-process {
    LIBS += -ldl
}

HEADERS += \
    abstractapplicationmanager.h \
    application.h \
    applicationmanager.h \
    applicationmodel.h \
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
    applicationmanager_p.h \
    systemreader.h \
    debugwrapper.h \

linux:HEADERS += \
    sysfsreader.h \

!headless:HEADERS += \
    fakeapplicationmanagerwindow.h \
    inprocesssurfaceitem.h

multi-process:HEADERS += \
    nativeruntime.h \
    nativeruntime_p.h \

qtHaveModule(qml):HEADERS += \
    qmlinprocessruntime.h \
    qmlinprocessapplicationinterface.h \

SOURCES += \
    application.cpp \
    applicationmanager.cpp \
    applicationmodel.cpp \
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
    systemreader.cpp \
    debugwrapper.cpp \

linux:SOURCES += \
    sysfsreader.cpp \

!headless:SOURCES += \
    fakeapplicationmanagerwindow.cpp \
    inprocesssurfaceitem.cpp

multi-process:SOURCES += \
    nativeruntime.cpp \

qtHaveModule(qml):SOURCES += \
    qmlinprocessruntime.cpp \
    qmlinprocessapplicationinterface.cpp \

# we have an external plugin interface with signals, so we need to
# compile the moc-data into the exporting binary (appman itself)
HEADERS += ../plugin-interfaces/containerinterface.h

load(qt_module)
