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
    systemmonitor_p.h \
    processmonitor.h \
    processmonitor_p.h \
    frametimer.h

SOURCES += \
    systemmonitor.cpp \
    processmonitor.cpp \
    processmonitor_p.cpp \
    frametimer.cpp

load(qt_module)
