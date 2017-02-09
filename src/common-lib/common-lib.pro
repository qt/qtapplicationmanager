TARGET = QtAppManCommon
MODULE = appman_common

load(am-config)

QT = core network
qtHaveModule(geniviextras):QT *= geniviextras
qtHaveModule(dbus):QT *= dbus
qtHaveModule(qml):QT *= qml qml-private

!lessThan(QT.geniviextras.MAJOR_VERSION, 1) : !lessThan(QT.geniviextras.MINOR_VERSION, 1) {
    DEFINES += AM_GENIVIEXTRAS_LAZY_INIT
}

CONFIG *= static internal_module

include($$SOURCE_DIR/3rdparty/libyaml.pri)
contains(DEFINES, "AM_USE_LIBBACKTRACE"):include($$SOURCE_DIR/3rdparty/libbacktrace.pri)

SOURCES += \
    global.cpp \
    exception.cpp \
    utilities.cpp \
    qtyaml.cpp \
    startuptimer.cpp \
    dbus-policy.cpp \
    unixsignalhandler.cpp

qtHaveModule(qml):SOURCES += \
    qml-utilities.cpp \

qtHaveModule(dbus):SOURCES += \
    dbus-utilities.cpp \

HEADERS += \
    global.h \
    error.h \
    exception.h \
    utilities.h \
    qtyaml.h \
    startuptimer.h \
    dbus-policy.h \
    unixsignalhandler.h

qtHaveModule(qml):HEADERS += \
    qml-utilities.h \

qtHaveModule(dbus):HEADERS += \
    dbus-utilities.h \

# MingW does not add a dummy manifest, leading to UAC prompts on binaries containing "bad" words
# like setup, install, update, patch, ...
win32-g++*:RC_FILE = resources-win.rc
OTHER_FILES += resources-win.rc manifest-win.xml

load(qt_module)
