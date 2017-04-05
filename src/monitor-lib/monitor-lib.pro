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
    systemmonitor.h \
    processmonitor.h \
    processmonitor_p.h \
    xprocessmonitor.h \
    memorymonitor.h \
    frametimer.h

SOURCES += \
    systemmonitor.cpp \
    processmonitor.cpp \
    processmonitor_p.cpp \
    xprocessmonitor.cpp \
    memorymonitor.cpp \
    frametimer.cpp

load(qt_module)
