
TEMPLATE = lib
TARGET = installer-lib

load(am-config)

CONFIG += static create_prl

QT = core network qml
qtHaveModule(dbus):QT *= dbus

DEFINES *= AM_BUILD_APPMAN

load(add-static-library)
addStaticLibrary(../common-lib)
addStaticLibrary(../crypto-lib)
addStaticLibrary(../manager-lib)

include($$SOURCE_DIR/3rdparty/libarchive.pri)
include($$SOURCE_DIR/3rdparty/libz.pri)

HEADERS += \
    package_p.h \
    packageextractor_p.h \
    packageextractor.h \
    packagecreator_p.h \
    packagecreator.h \
    asynchronoustask.h \
    deinstallationtask.h \
    installationtask.h \
    scopeutilities.h \
    applicationinstaller.h \
    applicationinstaller_p.h \
    sudo.h \

SOURCES += \
    package_p.cpp \
    packagecreator.cpp \
    packageextractor.cpp \
    asynchronoustask.cpp \
    installationtask.cpp \
    deinstallationtask.cpp \
    scopeutilities.cpp \
    applicationinstaller.cpp \
    sudo.cpp \
