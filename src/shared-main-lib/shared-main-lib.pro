TEMPLATE = lib
TARGET = QtAppManSharedMain
MODULE = appman_shared_main

load(am-config)

QT = core network qml
!headless:QT *= gui gui-private
QT *= \
    appman_common-private \

CONFIG *= static internal_module

HEADERS += \
    $$PWD/sharedmain.h \
    $$PWD/qmllogger.h \

SOURCES += \
    $$PWD/sharedmain.cpp \
    $$PWD/qmllogger.cpp \

load(qt_module)
