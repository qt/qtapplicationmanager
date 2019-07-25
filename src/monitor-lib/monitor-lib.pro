TEMPLATE = lib
TARGET = QtAppManMonitor
MODULE = appman_monitor

load(am-config)

QT = core
!headless: QT *= gui

QT_FOR_PRIVATE *= \
    appman_common-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

linux:HEADERS += \
    sysfsreader.h \

HEADERS += \
    systemreader.h \
    processreader.h \

linux:SOURCES += \
    sysfsreader.cpp \

SOURCES += \
    systemreader.cpp \
    processreader.cpp \

load(qt_module)
