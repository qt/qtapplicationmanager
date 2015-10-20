
TEMPLATE = lib
TARGET = notification-lib

include($$BASE_PRI)

CONFIG += static create_prl

QT = core qml

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../common-lib)

HEADERS += \
    notification.h \

SOURCES += \
    notification.cpp \
