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
    iostatus.h \
    memorystatus.h \
    monitormodel.h \
    processreader.h \
    processstatus.h \

SOURCES += \
    cpustatus.cpp \
    iostatus.cpp \
    memorystatus.cpp \
    monitormodel.cpp \
    processreader.cpp \
    processstatus.cpp \

!headless:HEADERS += \
    frametimer.h \
    gpustatus.h \

!headless:SOURCES += \
    frametimer.cpp \
    gpustatus.cpp \


load(qt_module)
