TEMPLATE = lib
TARGET = QtAppManSharedMain
MODULE = appman_shared_main

load(am-config)

QT = core network qml
!headless:QT *= gui gui-private quick
QT *= \
    appman_common-private \
    appman_monitor-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

HEADERS += \
    sharedmain.h \
    qmllogger.h \
    cpustatus.h \
    iostatus.h \
    memorystatus.h \
    monitormodel.h \

SOURCES += \
    sharedmain.cpp \
    qmllogger.cpp \
    cpustatus.cpp \
    iostatus.cpp \
    memorystatus.cpp \
    monitormodel.cpp \

!headless:HEADERS += \
    frametimer.h \
    gpustatus.h \

!headless:SOURCES += \
    frametimer.cpp \
    gpustatus.cpp \

load(qt_module)
