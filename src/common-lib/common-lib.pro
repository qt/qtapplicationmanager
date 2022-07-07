TEMPLATE = lib
TARGET = QtAppManCommon
MODULE = appman_common

load(am-config)

QT = core core-private network concurrent
qtHaveModule(geniviextras):QT *= geniviextras
android:QT *= androidextras
qtHaveModule(dbus):QT *= dbus
qtHaveModule(qml):QT *= qml qml-private

linux:LIBS += -ldl
qnx:LIBS += -lbacktrace

versionAtLeast(QT.geniviextras.VERSION, 1.1.0) {
    DEFINES += AM_GENIVIEXTRAS_LAZY_INIT
}

CONFIG *= static internal_module
CONFIG -= create_cmake

include($$SOURCE_DIR/3rdparty/libyaml.pri)
contains(DEFINES, "AM_USE_LIBBACKTRACE"):include($$SOURCE_DIR/3rdparty/libbacktrace.pri)
contains(DEFINES, "AM_USE_STACKWALKER"):include($$SOURCE_DIR/3rdparty/stackwalker.pri)

SOURCES += \
    exception.cpp \
    filesystemmountwatcher.cpp \
    utilities.cpp \
    qtyaml.cpp \
    startuptimer.cpp \
    unixsignalhandler.cpp \
    processtitle.cpp \
    crashhandler.cpp \
    logging.cpp \
    dbus-utilities.cpp \
    configcache.cpp

qtHaveModule(qml):SOURCES += \
    qml-utilities.cpp \

HEADERS += \
    global.h \
    error.h \
    exception.h \
    filesystemmountwatcher.h \
    utilities.h \
    qtyaml.h \
    startuptimer.h \
    unixsignalhandler.h \
    processtitle.h \
    crashhandler.h \
    logging.h \
    configcache.h \
    configcache_p.h

qtHaveModule(qml):HEADERS += \
    qml-utilities.h \

qtHaveModule(dbus):HEADERS += \
    dbus-utilities.h \

load(qt_module)
