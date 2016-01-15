
TEMPLATE = lib
TARGET = notification-lib

load(am-config)

CONFIG += static create_prl

QT = core qml

DEFINES *= AM_BUILD_APPMAN

load(add-static-library)
addStaticLibrary(../common-lib)

HEADERS += \
    notification.h \

SOURCES += \
    notification.cpp \
