TEMPLATE = lib
TARGET = QtAppManMonitor
MODULE = appman_monitor

load(am-config)

QT = core qml

QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_window-private \

CONFIG *= static internal_module

HEADERS += \
    cpustatus.h \
    frametimer.h \
    gpustatus.h \
    iostatus.h \
    memorystatus.h \
    monitormodel.h \
    processreader.h \
    processstatus.h \

SOURCES += \
    cpustatus.cpp \
    frametimer.cpp \
    gpustatus.cpp \
    iostatus.cpp \
    memorystatus.cpp \
    monitormodel.cpp \
    processreader.cpp \
    processstatus.cpp \

load(qt_module)
