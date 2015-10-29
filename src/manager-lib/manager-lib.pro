
TEMPLATE = lib
TARGET = manager-lib

include($$BASE_PRI)

CONFIG += static create_prl

QT = core network qml qml-private
!headless:QT *= gui quick

qtHaveModule(dbus) {
    packagesExist(dbus-1) {
        PKGCONFIG *= dbus-1
        DEFINES *= AM_LIBDBUS_AVAILABLE
    }
    QT *= dbus
}

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../common-lib)
addStaticLibrary(../crypto-lib)
addStaticLibrary(../notification-lib)

headers.path= $$INSTALL_PREFIX/include/application-manager
headers.files = abstractruntime.h application.h applicationmanager.h applicationinterface.h runtimefactory.h executioncontainerfactory.h executioncontainer.h ../common-lib/global.h

INSTALLS += headers

HEADERS += \
    application.h \
    applicationdatabase.h \
    applicationmanager.h \
    applicationscanner.h \
    executioncontainer.h \
    jsonapplicationscanner.h \
    runtimefactory.h \
    executioncontainerfactory.h \
    applicationinterface.h \
    abstractruntime.h \
    yamlapplicationscanner.h \
    installationlocation.h \
    installationreport.h \
    dbus-utilities.h \
    notificationmanager.h \
    qmlinprocessruntime.h \
    qmlinprocessapplicationinterface.h \
    qml-utilities.h

!headless:HEADERS += \
    fakepelagicorewindow.h \
    window.h \


qtHaveModule(dbus):HEADERS += \
    nativeruntime.h \
    nativeruntime_p.h \

SOURCES += \
    application.cpp \
    applicationdatabase.cpp \
    applicationmanager.cpp \
    executioncontainer.cpp \
    jsonapplicationscanner.cpp \
    runtimefactory.cpp \
    executioncontainerfactory.cpp \
    abstractruntime.cpp \
    yamlapplicationscanner.cpp \
    installationlocation.cpp \
    installationreport.cpp \
    dbus-utilities.cpp \
    notificationmanager.cpp \
    qmlinprocessruntime.cpp \
    qmlinprocessapplicationinterface.cpp \
    qml-utilities.cpp

!headless:SOURCES += \
    fakepelagicorewindow.cpp \
    window.cpp \

qtHaveModule(dbus):SOURCES += \
    nativeruntime.cpp \
