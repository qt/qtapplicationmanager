
TEMPLATE = lib
TARGET = common-lib

load(am-config)

CONFIG *= static create_prl

QT = core network
qtHaveModule(geniviextras):QT *= geniviextras
qtHaveModule(dbus):QT *= dbus
qtHaveModule(qml):QT *= qml

DEFINES *= AM_BUILD_APPMAN

include($$SOURCE_DIR/3rdparty/libyaml.pri)

SOURCES += \
    exception.cpp \
    global.cpp \
    utilities.cpp \
    qtyaml.cpp \
    startuptimer.cpp \
    dbus-utilities.cpp \

HEADERS += \
    error.h \
    exception.h \
    global.h \
    utilities.h \
    qtyaml.h \
    startuptimer.h \
    dbus-utilities.h \
