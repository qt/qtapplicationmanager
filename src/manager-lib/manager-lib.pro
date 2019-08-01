TEMPLATE = lib
TARGET = QtAppManManager
MODULE = appman_manager

load(am-config)

QT = core network qml
!headless:QT *= gui gui-private quick qml-private quick-private
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_application-private \
    appman_notification-private \
    appman_plugininterfaces-private \
    appman_intent_server-private \
    appman_intent_client-private \
    appman_monitor-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

multi-process {
    QT *= dbus
    LIBS *= -ldl

    HEADERS += \
        nativeruntime.h \
        nativeruntime_p.h \
        processcontainer.h \

    SOURCES += \
        nativeruntime.cpp \
        processcontainer.cpp \

    CONFIG = dbus-adaptors-xml $$CONFIG

    ADAPTORS_XML = \
        ../dbus-lib/io.qt.applicationmanager.intentinterface.xml
}

HEADERS += \
    application.h \
    applicationmanager.h \
    applicationmodel.h \
    notificationmanager.h \
    abstractcontainer.h \
    containerfactory.h \
    plugincontainer.h \
    abstractruntime.h \
    runtimefactory.h \
    quicklauncher.h \
    applicationipcmanager.h \
    applicationipcinterface.h \
    applicationipcinterface_p.h \
    applicationmanager_p.h \
    debugwrapper.h \
    amnamespace.h \
    intentaminterface.h \
    processstatus.h \
    package.h \
    packagemanager.h \
    packagemanager_p.h \

!headless:HEADERS += \
    qmlinprocessapplicationmanagerwindow.h \
    inprocesssurfaceitem.h

qtHaveModule(qml):HEADERS += \
    qmlinprocessruntime.h \
    qmlinprocessapplicationinterface.h \

SOURCES += \
    application.cpp \
    applicationmanager.cpp \
    applicationmodel.cpp \
    notificationmanager.cpp \
    abstractcontainer.cpp \
    containerfactory.cpp \
    plugincontainer.cpp \
    abstractruntime.cpp \
    runtimefactory.cpp \
    quicklauncher.cpp \
    applicationipcmanager.cpp \
    applicationipcinterface.cpp \
    debugwrapper.cpp \
    intentaminterface.cpp \
    processstatus.cpp \
    packagemanager.cpp \
    package.cpp \

!headless:SOURCES += \
    qmlinprocessapplicationmanagerwindow.cpp \
    inprocesssurfaceitem.cpp

qtHaveModule(qml):SOURCES += \
    qmlinprocessruntime.cpp \
    qmlinprocessapplicationinterface.cpp \

# we have an external plugin interface with signals, so we need to
# compile the moc-data into the exporting binary (appman itself)
HEADERS += ../plugin-interfaces/containerinterface.h


!disable-installer {

    QT_FOR_PRIVATE *= \
        appman_package-private \
        appman_crypto-private \

    HEADERS += \
        asynchronoustask.h \
        deinstallationtask.h \
        installationtask.h \
        scopeutilities.h \
        sudo.h \

    SOURCES += \
        asynchronoustask.cpp \
        installationtask.cpp \
        deinstallationtask.cpp \
        scopeutilities.cpp \
        sudo.cpp \
}

load(qt_module)
