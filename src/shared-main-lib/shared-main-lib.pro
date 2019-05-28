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

HEADERS += \
    sharedmain.h \
    qmllogger.h \
    cpustatus.h \
    frametimer.h \
    gpustatus.h \
    iostatus.h \
    memorystatus.h \
    monitormodel.h \

SOURCES += \
    sharedmain.cpp \
    qmllogger.cpp \
    cpustatus.cpp \
    frametimer.cpp \
    gpustatus.cpp \
    iostatus.cpp \
    memorystatus.cpp \
    monitormodel.cpp \

load(qt_module)
