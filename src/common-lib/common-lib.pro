
TEMPLATE = lib
TARGET = common-lib

include($$BASE_PRI)

CONFIG *= static create_prl

QT = core network
qtHaveModule(geniviextras):QT *= geniviextras

DEFINES *= AM_BUILD_APPMAN

include($$SOURCE_DIR/3rdparty/libyaml.pri)

SOURCES += \
    exception.cpp \
    global.cpp \
    utilities.cpp \
    qtyaml.cpp \
    startuptimer.cpp

HEADERS += \
    error.h \
    exception.h \
    global.h \
    utilities.h \
    qtyaml.h \
    startuptimer.h
