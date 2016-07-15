
TEMPLATE = lib
TARGET = manager-lib

load(am-config)

CONFIG += static create_prl

QT = core network qml qml-private
!headless:QT *= gui quick
qtHaveModule(dbus):QT *= dbus

multi-process {
    PKGCONFIG += "'dbus-1 >= 1.8'"
}

DEFINES *= AM_BUILD_APPMAN

load(add-static-library)
addStaticLibrary(../common-lib)
addStaticLibrary(../crypto-lib)
addStaticLibrary(../notification-lib)

HEADERS += \
    application.h \
    applicationdatabase.h \
    applicationmanager.h \
    applicationscanner.h \
    runtimefactory.h \
    applicationinterface.h \
    abstractruntime.h \
    yamlapplicationscanner.h \
    installationlocation.h \
    installationreport.h \
    dbus-policy.h \
    notificationmanager.h \
    qmlinprocessruntime.h \
    qmlinprocessapplicationinterface.h \
    qml-utilities.h \
    processcontainer.h \
    abstractcontainer.h \
    containerfactory.h \
    quicklauncher.h \
    systemmonitor.h \
    systemmonitor_p.h \
    applicationipcmanager.h \
    applicationipcinterface.h \
    applicationipcinterface_p.h \

!headless:HEADERS += \
    fakeapplicationmanagerwindow.h \
    window.h \

multi-process:HEADERS += \
    nativeruntime.h \
    nativeruntime_p.h \

SOURCES += \
    application.cpp \
    applicationdatabase.cpp \
    applicationmanager.cpp \
    runtimefactory.cpp \
    applicationinterface.cpp \
    abstractruntime.cpp \
    yamlapplicationscanner.cpp \
    installationlocation.cpp \
    installationreport.cpp \
    dbus-policy.cpp \
    notificationmanager.cpp \
    qmlinprocessruntime.cpp \
    qmlinprocessapplicationinterface.cpp \
    qml-utilities.cpp \
    processcontainer.cpp \
    abstractcontainer.cpp \
    containerfactory.cpp \
    quicklauncher.cpp \
    systemmonitor.cpp \
    systemmonitor_p.cpp \
    applicationipcmanager.cpp \
    applicationipcinterface.cpp

!headless:SOURCES += \
    fakeapplicationmanagerwindow.cpp \
    window.cpp \

multi-process:SOURCES += \
    nativeruntime.cpp \
